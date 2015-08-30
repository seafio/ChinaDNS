#include <fcntl.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>

#include "log.h"

#define PID_BUF_SIZE 32

static int write_pid_file(const char *filename, pid_t pid) {
  char buf[PID_BUF_SIZE];
  int fd = open(filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    errf("can not open %s", filename);
    err("open");
    return -1;
  }
  int flags = fcntl(fd, F_GETFD);
  if (flags == -1) {
    err("fcntl");
    return -1;
  }

  flags |= FD_CLOEXEC;
  if (-1 == fcntl(fd, F_SETFD, flags))
    err("fcntl");

  struct flock fl;
  fl.l_type = F_WRLCK;
  fl.l_whence = SEEK_SET;
  fl.l_start = 0;
  fl.l_len = 0;
  if (-1 == fcntl(fd, F_SETLK, &fl)) {
    ssize_t n = read(fd, buf, PID_BUF_SIZE - 1);
    if (n > 0) {
      buf[n] = 0;
      errf("already started at pid %ld", atol(buf));
    } else {
      errf("already started");
    }
    close(fd);
    return -1;
  }
  if (-1 == ftruncate(fd, 0)) {
    err("ftruncate");
    return -1;
  }
  snprintf(buf, PID_BUF_SIZE, "%ld\n", (long)getpid());

  if (write(fd, buf, strlen(buf)) != strlen(buf)) {
    err("write");
    return -1;
  }
  return 0;
}

int daemon_start(const char *logfile, const char *pidfile) {
#ifndef __APPLE__
    daemon (1, 0);
#else   /* __APPLE */
    /* daemon is deprecated under APPLE
        * use fork() instead
        * */
    switch (fork ()) {
    case -1:
        printf ("Failed to daemonize\n");
        exit (-1);
        break;
    case 0:
        /* all good*/
        break;
    default:
        /* kill origin process */
        printf("started\n");
        exit (0);
    }
#endif

  pid_t pid = getpid();
  if (0 != write_pid_file(pidfile, pid)) {
    return -1;
  }

  setsid();
  signal(SIGHUP, SIG_IGN);

  // then rediret stdout & stderr
  fclose(stdin);
  FILE *fp;
  fp = freopen(logfile, "a", stdout);
  if (fp == NULL) {
    perror("freopen");
    return -1;
  }
  fp = freopen(logfile, "a", stderr);
  if (fp == NULL) {
    perror("freopen");
    return -1;
  }

  logf("daemon started");

  return 0;
}
