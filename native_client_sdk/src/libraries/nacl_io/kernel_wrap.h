// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_KERNEL_WRAP_H_
#define LIBRARIES_NACL_IO_KERNEL_WRAP_H_

#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include "nacl_io/ossocket.h"
#include "nacl_io/ostypes.h"
#include "nacl_io/osutime.h"
#include "sdk_util/macros.h"

#if defined(__GLIBC__)
#include <sys/cdefs.h>
#define NOTHROW __THROW
#else
#define NOTHROW
#endif

#if defined(WIN32)
typedef int chmod_mode_t;
typedef int getcwd_size_t;
typedef int read_ssize_t;
typedef int write_ssize_t;
#define NAME(x) _##x
#else
typedef mode_t chmod_mode_t;
typedef size_t getcwd_size_t;
typedef ssize_t read_ssize_t;
typedef ssize_t write_ssize_t;
#define NAME(x) x
#endif

EXTERN_C_BEGIN

void kernel_wrap_init();
void kernel_wrap_uninit();

int NAME(access)(const char* path, int amode) NOTHROW;
int NAME(chdir)(const char* path) NOTHROW;
int NAME(chmod)(const char* path, chmod_mode_t mode) NOTHROW;
int chown(const char* path, uid_t owner, gid_t group) NOTHROW;
int NAME(close)(int fd);
int NAME(dup)(int oldfd) NOTHROW;
int NAME(dup2)(int oldfd, int newfd) NOTHROW;
int fchown(int fd, uid_t owner, gid_t group) NOTHROW;
#if defined(WIN32)
int _fstat32(int fd, struct _stat32* buf);
int _fstat64(int fd, struct _stat64* buf);
int _fstat32i64(int fd, struct _stat32i64* buf);
int _fstat64i32(int fd, struct _stat64i32* buf);
#else
struct stat;
int fstat(int fd, struct stat* buf) NOTHROW;
#endif
int fsync(int fd);
int ftruncate(int fd, off_t length) NOTHROW;
char* NAME(getcwd)(char* buf, getcwd_size_t size) NOTHROW;
char* getwd(char* buf) NOTHROW;
int getdents(int fd, void* buf, unsigned int count) NOTHROW;
int NAME(isatty)(int fd) NOTHROW;
int lchown(const char* path, uid_t owner, gid_t group) NOTHROW;
int link(const char* oldpath, const char* newpath) NOTHROW;
off_t NAME(lseek)(int fd, off_t offset, int whence) NOTHROW;
#if defined(WIN32)
int _mkdir(const char* path);
#else
int mkdir(const char* path, mode_t mode) NOTHROW;
#endif
void* mmap(void* addr, size_t length, int prot, int flags, int fd,
           off_t offset) NOTHROW;
int mount(const char* source, const char* target, const char* filesystemtype,
          unsigned long mountflags, const void* data) NOTHROW;
int munmap(void* addr, size_t length) NOTHROW;
int NAME(open)(const char* path, int oflag, ...);
read_ssize_t NAME(read)(int fd, void* buf, size_t nbyte);
int remove(const char* path) NOTHROW;
int NAME(rmdir)(const char* path) NOTHROW;
sighandler_t sigset(int sig, sighandler_t disp);
#if defined(WIN32)
int setenv(const char* name, const char* value, int overwrite);
int _stat32(const char* path, struct _stat32* buf);
int _stat64(const char* path, struct _stat64* buf);
int _stat32i64(const char* path, struct _stat32i64* buf);
int _stat64i32(const char* path, struct _stat64i32* buf);
#else
int stat(const char* path, struct stat* buf) NOTHROW;
#endif
int symlink(const char* oldpath, const char* newpath) NOTHROW;
int umount(const char* path) NOTHROW;
int NAME(unlink)(const char* path) NOTHROW;
uint64_t usec_since_epoch();
int utime(const char* filename, const struct utimbuf* times);
read_ssize_t NAME(write)(int fd, const void* buf, size_t nbyte);

#ifdef PROVIDES_SOCKET_API
// Socket Functions
int accept(int fd, struct sockaddr* addr, socklen_t* len);
int bind(int fd, const struct sockaddr* addr, socklen_t len);
int connect(int fd, const struct sockaddr* addr, socklen_t len);
struct hostent* gethostbyname(const char* name);
int getpeername(int fd, struct sockaddr* addr, socklen_t* len);
int getsockname(int fd, struct sockaddr* addr, socklen_t* len);
int getsockopt(int fd, int lvl, int optname, void* optval, socklen_t* len);
int listen(int fd, int backlog);
ssize_t recv(int fd, void* buf, size_t len, int flags);
ssize_t recvfrom(int fd, void* buf, size_t len, int flags,
                 struct sockaddr* addr, socklen_t* addrlen);
ssize_t recvmsg(int fd, struct msghdr* msg, int flags);
ssize_t send(int fd, const void* buf, size_t len, int flags);
ssize_t sendto(int fd, const void* buf, size_t len, int flags,
               const struct sockaddr* addr, socklen_t addrlen);
ssize_t sendmsg(int fd, const struct msghdr* msg, int flags);
int setsockopt(int fd, int lvl, int optname, const void* optval,
               socklen_t len);
int shutdown(int fd, int how);
int socket(int domain, int type, int protocol);
int socketpair(int domain, int type, int protocl, int* sv);
#endif  // PROVIDES_SOCKET_API

EXTERN_C_END

#endif  // LIBRARIES_NACL_IO_KERNEL_WRAP_H_
