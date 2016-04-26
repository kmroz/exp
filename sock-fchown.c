/*
 * sock-fchown.c
 *
 * Basic test of fchown() vs chown() on a UNIX domain socket file.
 *
 * Author: Karol Mroz
 *
 * Findings: fchown() fails to properly set the ownership on a socket file despite
 *           return 0. chown() is needed to properly set the permissions.
 */


#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>

void usage(char *program);
int parse_uid_gid_strings(const char *uid_str, const char *gid_str, uid_t *uid, gid_t *gid);

void
usage (char *program)
{
    printf("%s /some/socketfile uid gid\n", program);
}

int
parse_uid_gid_strings (const char *uid_str, const char *gid_str, uid_t *uid, gid_t *gid)
{
    struct passwd *pwd;
    struct group *grp;

    if (!(pwd = getpwnam(uid_str))) {
        return -1;
    }

    if (!(grp = getgrnam(gid_str))) {
        return -1;
    }

    *uid = pwd->pw_uid;
    *gid = grp->gr_gid;

    return 0;
}

int
main (int argc, char **argv)
{
    const char *path = argv[1];
    const char *uid_str = argv[2];
    const char *gid_str = argv[3];

    if (argc < 4) {
        usage(argv[0]);
        return 1;
    }

    struct sockaddr_un address;
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, path);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    bind(fd, (struct sockaddr*)(&address), sizeof(address));
    if (fd < 0) {
        char *e_msg = NULL;
        asprintf(&e_msg, "Failed to create socket %s", path);
        perror(e_msg);
        return fd;
    }

    int r = 0; /* SUCCESS by default. */
    uid_t uid;
    gid_t gid;
    r = parse_uid_gid_strings(uid_str, gid_str, &uid, &gid);
    if (r < 0) {
        printf("Failed to obtain uid:gid of %s:%s\n", uid_str, gid_str);
        goto cleanup;
    }
    
    printf("Calling fchown(%d, %d, %d)\n", fd, uid, gid);
    r = fchown(fd, uid, gid);
    if (r < 0) {
        char *e_msg = NULL;
        asprintf(&e_msg, "Failed to fchown %s", path);
        perror(e_msg);
        goto cleanup;
    } else {
        printf("fchown() SUCCESS\n");
    }

    struct stat buf;
    r = stat(path, &buf);
    if (r < 0) {
        char *e_msg = NULL;
        asprintf(&e_msg, "Failed to stat %s", path);
        perror(e_msg);
        goto cleanup;
    }
    printf("New uid:gid (%d:%d)\n", buf.st_uid, buf.st_gid);
    if ((uid != buf.st_uid) || (gid != buf.st_gid)) {
        printf("ERROR: Failed to properly set uid or gid.\n");
    }

    printf("Calling chown(%d, %d, %d)\n", fd, uid, gid);
    r = chown(path, uid, gid);
    if (r < 0) {
        char *e_msg = NULL;
        asprintf(&e_msg, "Failed to chown %s", path);
        perror(e_msg);
        goto cleanup;
    } else {
        printf("chown() SUCCESS\n");
    }

    r = stat(path, &buf);
    if (r < 0) {
        char *e_msg = NULL;
        asprintf(&e_msg, "Failed to stat %s", path);
        perror(e_msg);
        goto cleanup;
    }
    printf("New uid:gid (%d:%d)\n", buf.st_uid, buf.st_gid);
    if ((uid != buf.st_uid) || (gid != buf.st_gid)) {
        printf("ERROR: Failed to properly set uid or gid.\n");
    }

cleanup:
    printf("Removing %s.\n", path);
    int s = remove(path);
    if (s < 0) {
        char *e_msg = NULL;
        asprintf(&e_msg, "Failed to remove %s", path);
        perror(e_msg);
        return s;
    }

    printf("All done\n");
    return r;
}
