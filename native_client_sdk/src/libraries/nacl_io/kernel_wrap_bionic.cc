// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>  // Include something that will define __BIONIC__.

// The entire file is wrapped in this #if. We do this so this .cc file can be
// compiled, even on a non-bionic build.

#if defined(__native_client__) && defined(__BIONIC__)
#include <alloca.h>
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <irt_syscalls.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_wrap.h"
#include "nacl_io/kernel_wrap_real.h"
#include "nacl_io/osmman.h"

namespace {

void stat_to_nacl_stat(const struct stat* buf, nacl_abi_stat* nacl_buf) {
  memset(nacl_buf, 0, sizeof(struct nacl_abi_stat));
  nacl_buf->nacl_abi_st_dev = buf->st_dev;
  nacl_buf->nacl_abi_st_ino = buf->st_ino;
  nacl_buf->nacl_abi_st_mode = buf->st_mode;
  nacl_buf->nacl_abi_st_nlink = buf->st_nlink;
  nacl_buf->nacl_abi_st_uid = buf->st_uid;
  nacl_buf->nacl_abi_st_gid = buf->st_gid;
  nacl_buf->nacl_abi_st_rdev = buf->st_rdev;
  nacl_buf->nacl_abi_st_size = buf->st_size;
  nacl_buf->nacl_abi_st_blksize = buf->st_blksize;
  nacl_buf->nacl_abi_st_blocks = buf->st_blocks;
  nacl_buf->nacl_abi_st_atime = buf->st_atime;
  nacl_buf->nacl_abi_st_mtime = buf->st_mtime;
  nacl_buf->nacl_abi_st_ctime = buf->st_ctime;
}

void nacl_stat_to_stat(const nacl_abi_stat* nacl_buf, struct stat* buf) {
  memset(buf, 0, sizeof(struct stat));
  buf->st_dev = nacl_buf->nacl_abi_st_dev;
  buf->st_ino = nacl_buf->nacl_abi_st_ino;
  buf->st_mode = nacl_buf->nacl_abi_st_mode;
  buf->st_nlink = nacl_buf->nacl_abi_st_nlink;
  buf->st_uid = nacl_buf->nacl_abi_st_uid;
  buf->st_gid = nacl_buf->nacl_abi_st_gid;
  buf->st_rdev = nacl_buf->nacl_abi_st_rdev;
  buf->st_size = nacl_buf->nacl_abi_st_size;
  buf->st_blksize = nacl_buf->nacl_abi_st_blksize;
  buf->st_blocks = nacl_buf->nacl_abi_st_blocks;
  buf->st_atime = nacl_buf->nacl_abi_st_atime;
  buf->st_mtime = nacl_buf->nacl_abi_st_mtime;
  buf->st_ctime = nacl_buf->nacl_abi_st_ctime;
}

}  // namespace

// From native_client/src/trusted/service_runtime/include/sys/dirent.h

#ifndef nacl_abi___ino_t_defined
#define nacl_abi___ino_t_defined
typedef int64_t nacl_abi___ino_t;
typedef nacl_abi___ino_t nacl_abi_ino_t;
#endif

#ifndef nacl_abi___off_t_defined
#define nacl_abi___off_t_defined
typedef int64_t nacl_abi__off_t;
typedef nacl_abi__off_t nacl_abi_off_t;
#endif

/* We need a way to define the maximum size of a name. */
#ifndef MAXNAMLEN
# ifdef NAME_MAX
#  define MAXNAMLEN NAME_MAX
# else
#  define MAXNAMLEN 255
# endif
#endif

struct nacl_abi_dirent {
  nacl_abi_ino_t nacl_abi_d_ino;
  nacl_abi_off_t nacl_abi_d_off;
  uint16_t       nacl_abi_d_reclen;
  char           nacl_abi_d_name[MAXNAMLEN + 1];
};

static const int d_name_shift = offsetof (dirent, d_name) -
  offsetof (struct nacl_abi_dirent, nacl_abi_d_name);

EXTERN_C_BEGIN

// Macro to get the REAL function pointer
#define REAL(name) __nacl_irt_##name##_real

// Macro to get the WRAP function
#define WRAP(name) __nacl_irt_##name##_wrap

// Declare REAL function pointer.
#define DECLARE_REAL_PTR(name) typeof(__nacl_irt_##name) REAL(name);

// Assign the REAL function pointer.
#define ASSIGN_REAL_PTR(name) REAL(name) = __nacl_irt_##name;

// Switch IRT's pointer to the REAL pointer
#define USE_REAL(name) __nacl_irt_##name = (typeof(__nacl_irt_##name))REAL(name)

// Switch IRT's pointer to the WRAP function
#define USE_WRAP(name) __nacl_irt_##name = (typeof(__nacl_irt_##name))WRAP(name)

#define EXPAND_SYMBOL_LIST_OPERATION(OP) \
  OP(chdir);                             \
  OP(close);                             \
  OP(dup);                               \
  OP(dup2);                              \
  OP(exit);                              \
  OP(fchdir);                            \
  OP(fchmod);                            \
  OP(fdatasync);                         \
  OP(fstat);                             \
  OP(fsync);                             \
  OP(getcwd);                            \
  OP(getdents);                          \
  OP(isatty);                            \
  OP(lstat);                             \
  OP(mkdir);                             \
  OP(mmap);                              \
  OP(munmap);                            \
  OP(open);                              \
  OP(open_resource);                     \
  OP(poll);                              \
  OP(read);                              \
  OP(readlink);                          \
  OP(rmdir);                             \
  OP(seek);                              \
  OP(stat);                              \
  OP(truncate);                          \
  OP(write);

EXPAND_SYMBOL_LIST_OPERATION(DECLARE_REAL_PTR);

int WRAP(chdir)(const char* pathname) {
  ERRNO_RTN(ki_chdir(pathname));
}

int WRAP(close)(int fd) {
  ERRNO_RTN(ki_close(fd));
}

int WRAP(dup)(int fd, int* newfd) NOTHROW {
  *newfd = ki_dup(fd);
  ERRNO_RTN(*newfd);
}

int WRAP(dup2)(int fd, int newfd) NOTHROW {
  ERRNO_RTN(ki_dup2(fd, newfd));
}

void WRAP(exit)(int status) {
  ki_exit(status);
}

int WRAP(fchdir)(int fd) NOTHROW {
  ERRNO_RTN(ki_fchdir(fd));
}

int WRAP(fchmod)(int fd, mode_t mode) NOTHROW {
  ERRNO_RTN(ki_fchmod(fd, mode));
}

int WRAP(fdatasync)(int fd) NOTHROW {
  ERRNO_RTN(ki_fdatasync(fd));
}

int WRAP(fstat)(int fd, struct nacl_abi_stat* nacl_buf) {
  struct stat buf;
  memset(&buf, 0, sizeof(struct stat));
  int res = ki_fstat(fd, &buf);
  RTN_ERRNO_IF(res < 0);
  stat_to_nacl_stat(&buf, nacl_buf);
  return 0;
}

int WRAP(fsync)(int fd) NOTHROW {
  ERRNO_RTN(ki_fsync(fd));
}

int WRAP(getcwd)(char* buf, size_t size) {
  RTN_ERRNO_IF(ki_getcwd(buf, size) == NULL);
  return 0;
}

int WRAP(getdents)(int fd, dirent* nacl_buf, size_t nacl_count, size_t* nread) {
  int nacl_offset = 0;
  // "buf" contains dirent(s); "nacl_buf" contains nacl_abi_dirent(s).
  // nacl_abi_dirent(s) are smaller than dirent(s), so nacl_count bytes buffer
  // is enough
  char* buf = (char*)alloca(nacl_count);
  int offset = 0;
  int count;

  count = ki_getdents(fd, buf, nacl_count);
  RTN_ERRNO_IF(count < 0);

  while (offset < count) {
    dirent* d = (dirent*)(buf + offset);
    nacl_abi_dirent* nacl_d = (nacl_abi_dirent*)((char*)nacl_buf + nacl_offset);
    nacl_d->nacl_abi_d_ino = d->d_ino;
    nacl_d->nacl_abi_d_off = d->d_off;
    nacl_d->nacl_abi_d_reclen = d->d_reclen - d_name_shift;
    size_t d_name_len = d->d_reclen - offsetof(dirent, d_name);
    memcpy(nacl_d->nacl_abi_d_name, d->d_name, d_name_len);

    offset += d->d_reclen;
    nacl_offset += nacl_d->nacl_abi_d_reclen;
  }

  *nread = nacl_offset;
  return 0;
}

int WRAP(isatty)(int fd, int* result) {
  *result = ki_isatty(fd);
  RTN_ERRNO_IF(*result == 0);
  return 0;
}

int WRAP(lstat)(const char* path, struct nacl_abi_stat* nacl_buf) {
  struct stat buf;
  memset(&buf, 0, sizeof(struct stat));
  int res = ki_lstat(path, &buf);
  RTN_ERRNO_IF(res < 0);
  stat_to_nacl_stat(&buf, nacl_buf);
  return 0;
}

int WRAP(mkdir)(const char* pathname, mode_t mode) {
  ERRNO_RTN(ki_mkdir(pathname, mode));
}

int WRAP(mmap)(void** addr,
               size_t length,
               int prot,
               int flags,
               int fd,
               int64_t offset) {
  if (flags & MAP_ANONYMOUS)
    return REAL(mmap)(addr, length, prot, flags, fd, offset);

  *addr = ki_mmap(*addr, length, prot, flags, fd, offset);
  RTN_ERRNO_IF(*addr == (void*)-1)
  return 0;
}

int WRAP(munmap)(void* addr, size_t length) {
  // Always let the real munmap run on the address range. It is not an error if
  // there are no mapped pages in that range.
  ki_munmap(addr, length);
  return REAL(munmap)(addr, length);
}

int WRAP(open)(const char* pathname, int oflag, mode_t mode, int* newfd) {
  *newfd = ki_open(pathname, oflag, mode);
  ERRNO_RTN(*newfd);
}

int WRAP(open_resource)(const char* file, int* fd) {
  *fd = ki_open_resource(file);
  ERRNO_RTN(*fd);
}

int WRAP(poll)(struct pollfd* fds, nfds_t nfds, int timeout, int* count) {
  *count = ki_poll(fds, nfds, timeout);
  ERRNO_RTN(*count);
}

int WRAP(read)(int fd, void* buf, size_t count, size_t* nread) {
  ssize_t signed_nread = ki_read(fd, buf, count);
  *nread = static_cast<size_t>(signed_nread);
  ERRNO_RTN(signed_nread);
}

int WRAP(readlink)(const char* path, char* buf, size_t count, size_t* nread) {
  ssize_t signed_nread = ki_readlink(path, buf, count);
  *nread = static_cast<size_t>(signed_nread);
  ERRNO_RTN(signed_nread);
}

int WRAP(rmdir)(const char* pathname) {
  ERRNO_RTN(ki_rmdir(pathname));
}

int WRAP(seek)(int fd, off64_t offset, int whence, int64_t* new_offset) {
  *new_offset = ki_lseek(fd, offset, whence);
  ERRNO_RTN(*new_offset);
}

int WRAP(select)(int nfds,
                 fd_set* readfds,
                 fd_set* writefds,
                 fd_set* exceptfds,
                 struct timeval* timeout,
                 int* count) {
  *count = ki_select(nfds, readfds, writefds, exceptfds, timeout);
  ERRNO_RTN(*count);
}

int WRAP(stat)(const char* pathname, struct nacl_abi_stat* nacl_buf) {
  struct stat buf;
  memset(&buf, 0, sizeof(struct stat));
  int res = ki_stat(pathname, &buf);
  RTN_ERRNO_IF(res < 0);
  stat_to_nacl_stat(&buf, nacl_buf);
  return 0;
}

int WRAP(truncate)(const char* name, int64_t len) {
  ERRNO_RTN(ki_truncate(name, len));
}

int WRAP(write)(int fd, const void* buf, size_t count, size_t* nwrote) {
  ssize_t signed_nwrote = ki_write(fd, buf, count);
  *nwrote = static_cast<size_t>(signed_nwrote);
  ERRNO_RTN(signed_nwrote);
}

static void assign_real_pointers() {
  static bool assigned = false;
  if (!assigned) {
    EXPAND_SYMBOL_LIST_OPERATION(ASSIGN_REAL_PTR)
    assigned = true;
  }
}

#define CHECK_REAL(func) \
  if (!REAL(func)) {          \
    assign_real_pointers();   \
    if (!REAL(func))          \
      return ENOSYS;          \
  }

// "real" functions, i.e. the unwrapped original functions.

int _real_close(int fd) {
  CHECK_REAL(close);
  return REAL(close)(fd);
}

void _real_exit(int status) {
  REAL(exit)(status);
}

int _real_fchdir(int fd) {
  CHECK_REAL(fchdir);
  return REAL(fchdir)(fd);
}

int _real_fchmod(int fd, mode_t mode) {
  CHECK_REAL(fchmod);
  return REAL(fchmod)(fd, mode);
}

int _real_fdatasync(int fd) {
  CHECK_REAL(fdatasync);
  return REAL(fdatasync)(fd);
}

int _real_fstat(int fd, struct stat* buf) {
  struct nacl_abi_stat st;
  CHECK_REAL(fstat);

  int err = REAL(fstat)(fd, (struct stat*)&st);
  if (err) {
    errno = err;
    return -1;
  }

  nacl_stat_to_stat(&st, buf);
  return 0;
}

int _real_fsync(int fd) {
  CHECK_REAL(fsync);
  return REAL(fsync)(fd);
}

int _real_getdents(int fd, void* buf, size_t count, size_t* nread) {
  // "buf" contains dirent(s); "nacl_buf" contains nacl_abi_dirent(s).
  // See WRAP(getdents) above.
  char* nacl_buf = (char*)alloca(count);
  size_t offset = 0;
  size_t nacl_offset = 0;
  size_t nacl_nread;
  CHECK_REAL(getdents);
  int err = REAL(getdents)(fd, (dirent*)nacl_buf, count, &nacl_nread);
  if (err)
    return err;

  while (nacl_offset < nacl_nread) {
    dirent* d = (dirent*)((char*)buf + offset);
    nacl_abi_dirent* nacl_d = (nacl_abi_dirent*)(nacl_buf + nacl_offset);
    d->d_ino = nacl_d->nacl_abi_d_ino;
    d->d_off = nacl_d->nacl_abi_d_off;
    d->d_reclen = nacl_d->nacl_abi_d_reclen + d_name_shift;
    size_t d_name_len =
        nacl_d->nacl_abi_d_reclen - offsetof(nacl_abi_dirent, nacl_abi_d_name);
    memcpy(d->d_name, nacl_d->nacl_abi_d_name, d_name_len);

    offset += d->d_reclen;
    offset += nacl_d->nacl_abi_d_reclen;
  }

  *nread = offset;
  return 0;
}

int _real_isatty(int fd, int* result) {
  *result = isatty(fd);
  return *result ? 0 : -1;
}

int _real_lseek(int fd, int64_t offset, int whence, int64_t* new_offset) {
  CHECK_REAL(seek);
  nacl_abi_off_t nacl_new_offs;
  int ret = REAL(seek)(fd, offset, whence, &nacl_new_offs);
  *new_offset = static_cast<off_t>(nacl_new_offs);
  return ret;
}

int _real_lstat(const char* path, struct stat* buf) {
  struct nacl_abi_stat st;
  CHECK_REAL(lstat);

  int err = REAL(lstat)(path, (struct stat*)&st);
  if (err) {
    errno = err;
    return -1;
  }

  nacl_stat_to_stat(&st, buf);
  return 0;
}

int _real_mkdir(const char* pathname, mode_t mode) {
  CHECK_REAL(mkdir);
  return REAL(mkdir)(pathname, mode);
}

int _real_mmap(void** addr,
               size_t length,
               int prot,
               int flags,
               int fd,
               int64_t offset) {
  CHECK_REAL(mmap);
  return REAL(mmap)(addr, length, prot, flags, fd, offset);
}

int _real_munmap(void* addr, size_t length) {
  CHECK_REAL(munmap);
  return REAL(munmap)(addr, length);
}

int _real_open(const char* pathname, int oflag, mode_t mode, int* newfd) {
  CHECK_REAL(open);
  return REAL(open)(pathname, oflag, mode, newfd);
}

int _real_open_resource(const char* file, int* fd) {
  CHECK_REAL(open_resource);
  return REAL(open_resource)(file, fd);
}

int _real_read(int fd, void* buf, size_t count, size_t* nread) {
  CHECK_REAL(read);
  return REAL(read)(fd, buf, count, nread);
}

int _real_readlink(const char* path, char* buf, size_t count, size_t* nread) {
  CHECK_REAL(readlink);
  return REAL(readlink)(path, buf, count, nread);
}

int _real_rmdir(const char* pathname) {
  CHECK_REAL(rmdir);
  return REAL(rmdir)(pathname);
}

int _real_truncate(const char* pathname, int64_t len) {
  CHECK_REAL(truncate);
  return REAL(truncate)(pathname, len);
}

int _real_write(int fd, const void* buf, size_t count, size_t* nwrote) {
  CHECK_REAL(write);
  return REAL(write)(fd, buf, count, nwrote);
}

int _real_getcwd(char* pathname, size_t len) {
  CHECK_REAL(getcwd);
  return REAL(getcwd)(pathname, len);
}

static bool s_wrapped = false;

void kernel_wrap_init() {
  if (!s_wrapped) {
    assign_real_pointers();
    EXPAND_SYMBOL_LIST_OPERATION(USE_WRAP)
    s_wrapped = true;
  }
}

void kernel_wrap_uninit() {
  if (s_wrapped) {
    EXPAND_SYMBOL_LIST_OPERATION(USE_REAL)
    s_wrapped = false;
  }
}

EXTERN_C_END

#endif  // defined(__native_client__) && defined(__BIONIC__)
