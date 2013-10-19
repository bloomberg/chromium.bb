// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/kernel_intercept.h"

#include <errno.h>

#include "nacl_io/kernel_proxy.h"
#include "nacl_io/kernel_wrap.h"
#include "nacl_io/osmman.h"
#include "nacl_io/ossocket.h"
#include "nacl_io/pepper_interface.h"
#include "nacl_io/pepper_interface.h"
#include "nacl_io/real_pepper_interface.h"

using namespace nacl_io;

#define ON_NOSYS_RETURN(x)    \
  if (!ki_is_initialized()) { \
    errno = ENOSYS;           \
    return x;                 \
  }

static KernelProxy* s_kp;
static bool s_kp_owned;

void ki_init(void* kp) {
  ki_init_ppapi(kp, 0, NULL);
}

void ki_init_ppapi(void* kp,
                   PP_Instance instance,
                   PPB_GetInterface get_browser_interface) {
  kernel_wrap_init();

  if (kp == NULL) {
    s_kp = new KernelProxy();
    s_kp_owned = true;
  } else {
    s_kp = static_cast<KernelProxy*>(kp);
    s_kp_owned = false;
  }


  PepperInterface* ppapi = NULL;
  if (instance && get_browser_interface)
    ppapi = new RealPepperInterface(instance, get_browser_interface);

  s_kp->Init(ppapi);
}

int ki_is_initialized() {
  return s_kp != NULL;
}

void ki_uninit() {
  kernel_wrap_uninit();
  if (s_kp_owned)
    delete s_kp;
  s_kp = NULL;
}

int ki_chdir(const char* path) {
  ON_NOSYS_RETURN(-1);
  return s_kp->chdir(path);
}

char* ki_getcwd(char* buf, size_t size) {
  // gtest uses getcwd in a static initializer. If we haven't initialized the
  // kernel-intercept yet, just return ".".
  if (!ki_is_initialized()) {
    if (size < 2) {
      errno = ERANGE;
      return NULL;
    }
    buf[0] = '.';
    buf[1] = 0;
    return buf;
  }
  return s_kp->getcwd(buf, size);
}

char* ki_getwd(char* buf) {
  ON_NOSYS_RETURN(NULL);
  return s_kp->getwd(buf);
}

int ki_dup(int oldfd) {
  ON_NOSYS_RETURN(-1);
  return s_kp->dup(oldfd);
}

int ki_dup2(int oldfd, int newfd) {
  ON_NOSYS_RETURN(-1);
  return s_kp->dup2(oldfd, newfd);
}

int ki_chmod(const char *path, mode_t mode) {
  ON_NOSYS_RETURN(-1);
  return s_kp->chmod(path, mode);
}

int ki_fchdir(int fd) {
  ON_NOSYS_RETURN(-1);
  return s_kp->fchdir(fd);
}

int ki_fchmod(int fd, mode_t mode) {
  ON_NOSYS_RETURN(-1);
  return s_kp->fchmod(fd, mode);
}

int ki_stat(const char *path, struct stat *buf) {
  ON_NOSYS_RETURN(-1);
  return s_kp->stat(path, buf);
}

int ki_mkdir(const char *path, mode_t mode) {
  ON_NOSYS_RETURN(-1);
  return s_kp->mkdir(path, mode);
}

int ki_rmdir(const char *path) {
  ON_NOSYS_RETURN(-1);
  return s_kp->rmdir(path);
}

int ki_mount(const char *source, const char *target, const char *filesystemtype,
             unsigned long mountflags, const void *data) {
  ON_NOSYS_RETURN(-1);
  return s_kp->mount(source, target, filesystemtype, mountflags, data);
}

int ki_umount(const char *path) {
  ON_NOSYS_RETURN(-1);
  return s_kp->umount(path);
}

int ki_open(const char *path, int oflag) {
  ON_NOSYS_RETURN(-1);
  return s_kp->open(path, oflag);
}

int ki_pipe(int pipefds[2]) {
  ON_NOSYS_RETURN(-1);
  return s_kp->pipe(pipefds);
}

ssize_t ki_read(int fd, void *buf, size_t nbyte) {
  ON_NOSYS_RETURN(-1);
  return s_kp->read(fd, buf, nbyte);
}

ssize_t ki_write(int fd, const void *buf, size_t nbyte) {
  ON_NOSYS_RETURN(-1);
  return s_kp->write(fd, buf, nbyte);
}

int ki_fstat(int fd, struct stat *buf){
  ON_NOSYS_RETURN(-1);
  return s_kp->fstat(fd, buf);
}

int ki_getdents(int fd, void *buf, unsigned int count) {
  ON_NOSYS_RETURN(-1);
  return s_kp->getdents(fd, buf, count);
}

int ki_ftruncate(int fd, off_t length) {
  ON_NOSYS_RETURN(-1);
  return s_kp->ftruncate(fd, length);
}

int ki_fsync(int fd) {
  ON_NOSYS_RETURN(-1);
  return s_kp->fsync(fd);
}

int ki_fdatasync(int fd) {
  ON_NOSYS_RETURN(-1);
  return s_kp->fdatasync(fd);
}

int ki_isatty(int fd) {
  ON_NOSYS_RETURN(0);
  return s_kp->isatty(fd);
}

int ki_close(int fd) {
  ON_NOSYS_RETURN(-1);
  return s_kp->close(fd);
}

off_t ki_lseek(int fd, off_t offset, int whence) {
  ON_NOSYS_RETURN(-1);
  return s_kp->lseek(fd, offset, whence);
}

int ki_remove(const char* path) {
  ON_NOSYS_RETURN(-1);
  return s_kp->remove(path);
}

int ki_unlink(const char* path) {
  ON_NOSYS_RETURN(-1);
  return s_kp->unlink(path);
}

int ki_truncate(const char* path, off_t length) {
  ON_NOSYS_RETURN(-1);
  return s_kp->truncate(path, length);
}

int ki_lstat(const char* path, struct stat* buf) {
  ON_NOSYS_RETURN(-1);
  return s_kp->lstat(path, buf);
}

int ki_link(const char* oldpath, const char* newpath) {
  ON_NOSYS_RETURN(-1);
  return s_kp->link(oldpath, newpath);
}

int ki_rename(const char* path, const char* newpath) {
  ON_NOSYS_RETURN(-1);
  return s_kp->rename(path, newpath);
}

int ki_symlink(const char* oldpath, const char* newpath) {
  ON_NOSYS_RETURN(-1);
  return s_kp->symlink(oldpath, newpath);
}

int ki_access(const char* path, int amode) {
  ON_NOSYS_RETURN(-1);
  return s_kp->access(path, amode);
}

int ki_readlink(const char *path, char *buf, size_t count) {
  ON_NOSYS_RETURN(-1);
  return s_kp->readlink(path, buf, count);
}

int ki_utimes(const char *path, const struct timeval times[2]) {
  ON_NOSYS_RETURN(-1);
  return s_kp->utimes(path, times);
}

void* ki_mmap(void* addr, size_t length, int prot, int flags, int fd,
              off_t offset) {
  ON_NOSYS_RETURN(MAP_FAILED);
  return s_kp->mmap(addr, length, prot, flags, fd, offset);
}

int ki_munmap(void* addr, size_t length) {
  ON_NOSYS_RETURN(-1);
  return s_kp->munmap(addr, length);
}

int ki_open_resource(const char* file) {
  ON_NOSYS_RETURN(-1);  return s_kp->open_resource(file);
}

int ki_fcntl(int d, int request, va_list args) {
  ON_NOSYS_RETURN(-1);
  return s_kp->fcntl(d, request, args);
}

int ki_ioctl(int d, int request, va_list args) {
  ON_NOSYS_RETURN(-1);
  return s_kp->ioctl(d, request, args);
}

int ki_chown(const char* path, uid_t owner, gid_t group) {
  ON_NOSYS_RETURN(-1);
  return s_kp->chown(path, owner, group);
}

int ki_fchown(int fd, uid_t owner, gid_t group) {
  ON_NOSYS_RETURN(-1);
  return s_kp->fchown(fd, owner, group);
}

int ki_lchown(const char* path, uid_t owner, gid_t group) {
  ON_NOSYS_RETURN(-1);
  return s_kp->lchown(path, owner, group);
}

int ki_utime(const char* filename, const struct utimbuf* times) {
  ON_NOSYS_RETURN(-1);
  return s_kp->utime(filename, times);
}

int ki_poll(struct pollfd *fds, nfds_t nfds, int timeout) {
  return s_kp->poll(fds, nfds, timeout);
}

int ki_select(int nfds, fd_set* readfds, fd_set* writefds,
              fd_set* exceptfds, struct timeval* timeout) {
  return s_kp->select(nfds, readfds, writefds, exceptfds, timeout);
}

int ki_tcflush(int fd, int queue_selector) {
  ON_NOSYS_RETURN(-1);
  return s_kp->tcflush(fd, queue_selector);
}

int ki_tcgetattr(int fd, struct termios* termios_p) {
  ON_NOSYS_RETURN(-1);
  return s_kp->tcgetattr(fd, termios_p);
}

int ki_tcsetattr(int fd, int optional_actions,
                 const struct termios *termios_p) {
  ON_NOSYS_RETURN(-1);
  return s_kp->tcsetattr(fd, optional_actions, termios_p);
}

int ki_kill(pid_t pid, int sig) {
  ON_NOSYS_RETURN(-1);
  return s_kp->kill(pid, sig);
}

int ki_killpg(pid_t pid, int sig) {
  errno = ENOSYS;
  return -1;
}

int ki_sigaction(int, const struct sigaction*, struct sigaction*) {
  errno = ENOSYS;
  return -1;
}

int ki_sigpause(int sigmask) {
  errno = ENOSYS;
  return -1;
}

int ki_sigpending(sigset_t* set) {
  errno = ENOSYS;
  return -1;
}

int ki_sigsuspend(const sigset_t* set) {
  errno = ENOSYS;
  return -1;
}

sighandler_t ki_signal(int signum, sighandler_t handler) {
  ON_NOSYS_RETURN(SIG_ERR);
  return s_kp->sigset(signum, handler);
}

sighandler_t ki_sigset(int signum, sighandler_t handler) {
  ON_NOSYS_RETURN(SIG_ERR);
  return s_kp->sigset(signum, handler);
}

#ifdef PROVIDES_SOCKET_API
// Socket Functions
int ki_accept(int fd, struct sockaddr* addr, socklen_t* len) {
  return s_kp->accept(fd, addr, len);
}

int ki_bind(int fd, const struct sockaddr* addr, socklen_t len) {
  return s_kp->bind(fd, addr, len);
}

int ki_connect(int fd, const struct sockaddr* addr, socklen_t len) {
  return s_kp->connect(fd, addr, len);
}

struct hostent* ki_gethostbyname(const char* name) {
  return s_kp->gethostbyname(name);
}

int ki_getpeername(int fd, struct sockaddr* addr, socklen_t* len) {
  return s_kp->getpeername(fd, addr, len);
}

int ki_getsockname(int fd, struct sockaddr* addr, socklen_t* len) {
  return s_kp->getsockname(fd, addr, len);
}

int ki_getsockopt(int fd, int lvl, int optname, void* optval, socklen_t* len) {
  return s_kp->getsockopt(fd, lvl, optname, optval, len);
}

int ki_listen(int fd, int backlog) {
  return s_kp->listen(fd, backlog);
}

ssize_t ki_recv(int fd, void* buf, size_t len, int flags) {
  return s_kp->recv(fd, buf, len, flags);
}

ssize_t ki_recvfrom(int fd, void* buf, size_t len, int flags,
                 struct sockaddr* addr, socklen_t* addrlen) {
  return s_kp->recvfrom(fd, buf, len, flags, addr, addrlen);
}

ssize_t ki_recvmsg(int fd, struct msghdr* msg, int flags) {
  return s_kp->recvmsg(fd, msg, flags);
}

ssize_t ki_send(int fd, const void* buf, size_t len, int flags) {
  return s_kp->send(fd, buf, len, flags);
}

ssize_t ki_sendto(int fd, const void* buf, size_t len, int flags,
               const struct sockaddr* addr, socklen_t addrlen) {
  return s_kp->sendto(fd, buf, len, flags, addr, addrlen);
}

ssize_t ki_sendmsg(int fd, const struct msghdr* msg, int flags) {
  return s_kp->sendmsg(fd, msg, flags);
}

int ki_setsockopt(int fd, int lvl, int optname, const void* optval,
                  socklen_t len) {
  return s_kp->setsockopt(fd, lvl, optname, optval, len);
}

int ki_shutdown(int fd, int how) {
  return s_kp->shutdown(fd, how);
}

int ki_socket(int domain, int type, int protocol) {
  return s_kp->socket(domain, type, protocol);
}

int ki_socketpair(int domain, int type, int protocol, int* sv) {
  return s_kp->socketpair(domain, type, protocol, sv);
}
#endif  // PROVIDES_SOCKET_API
