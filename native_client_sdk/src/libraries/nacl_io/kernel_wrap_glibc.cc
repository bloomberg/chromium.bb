// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>  // Include something that will define __GLIBC__.

// The entire file is wrapped in this #if. We do this so this .cc file can be
// compiled, even on a non-glibc build.
#if defined(__native_client__) && defined(__GLIBC__)

#include "nacl_io/kernel_wrap.h"

#include <alloca.h>
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <irt.h>
#include <irt_syscalls.h>
#include <nacl_stat.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "nacl_io/kernel_intercept.h"
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
  buf->st_size = nacl_buf->nacl_abi_st_size ;
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
#define DECLARE_REAL_PTR(name) \
  typeof(__nacl_irt_##name) REAL(name);

// Assign the REAL function pointer.
#define ASSIGN_REAL_PTR(name) \
  assert(__nacl_irt_##name != NULL); \
  REAL(name) = __nacl_irt_##name;

// Switch IRT's pointer to the REAL pointer
#define USE_REAL(name) \
  __nacl_irt_##name = (typeof(__nacl_irt_##name)) REAL(name)

// Switch IRT's pointer to the WRAP function
#define USE_WRAP(name) \
  __nacl_irt_##name = (typeof(__nacl_irt_##name)) WRAP(name)


#define EXPAND_SYMBOL_LIST_OPERATION(OP) \
  OP(chdir); \
  OP(close); \
  OP(dup); \
  OP(dup2);  \
  OP(fstat); \
  OP(getcwd);  \
  OP(getdents);  \
  OP(mkdir); \
  OP(open); \
  OP(poll);\
  OP(read); \
  OP(rmdir); \
  OP(seek); \
  OP(stat); \
  OP(select); \
  OP(write); \
  OP(mmap); \
  OP(munmap); \
  OP(open_resource);

EXPAND_SYMBOL_LIST_OPERATION(DECLARE_REAL_PTR);

int WRAP(chdir) (const char* pathname) {
  return (ki_chdir(pathname)) ? errno : 0;
}

int WRAP(close)(int fd) {
  return (ki_close(fd) < 0) ? errno : 0;
}

int WRAP(dup)(int fd, int* newfd) NOTHROW {
  *newfd = ki_dup(fd);
  return (*newfd < 0) ? errno : 0;
}

int WRAP(dup2)(int fd, int newfd) NOTHROW {
  return (ki_dup2(fd, newfd) < 0) ? errno : 0;
}

int WRAP(fstat)(int fd, struct nacl_abi_stat *nacl_buf) {
  struct stat buf;
  memset(&buf, 0, sizeof(struct stat));
  int res = ki_fstat(fd, &buf);
  if (res < 0)
    return errno;
  stat_to_nacl_stat(&buf, nacl_buf);
  return 0;
}

char* WRAP(getcwd)(char* buf, size_t size) {
  return ki_getcwd(buf, size);
}

int WRAP(getdents)(int fd, dirent* nacl_buf, size_t nacl_count, size_t *nread) {
  int nacl_offset = 0;
  // "buf" contains dirent(s); "nacl_buf" contains nacl_abi_dirent(s).
  // nacl_abi_dirent(s) are smaller than dirent(s), so nacl_count bytes buffer
  // is enough
  char* buf = (char*)alloca(nacl_count);
  int offset = 0;
  int count;

  count = ki_getdents(fd, buf, nacl_count);
  if (count < 0)
    return errno;

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

int WRAP(mkdir)(const char* pathname, mode_t mode) {
  return (ki_mkdir(pathname, mode)) ? errno : 0;
}

int WRAP(mmap)(void** addr, size_t length, int prot, int flags, int fd,
               off_t offset) {
  if (flags & MAP_ANONYMOUS)
    return REAL(mmap)(addr, length, prot, flags, fd, offset);

  *addr = ki_mmap(*addr, length, prot, flags, fd, offset);
  return *addr == (void*)-1 ? errno : 0;
}

int WRAP(munmap)(void* addr, size_t length) {
  // Always let the real munmap run on the address range. It is not an error if
  // there are no mapped pages in that range.
  ki_munmap(addr, length);
  return REAL(munmap)(addr, length);
}

int WRAP(open)(const char* pathname, int oflag, mode_t cmode, int* newfd) {
  *newfd = ki_open(pathname, oflag);
  return (*newfd < 0) ? errno : 0;
}

int WRAP(open_resource)(const char* file, int* fd) {
  *fd = ki_open_resource(file);
  return (*fd < 0) ? errno : 0;
}

int WRAP(poll)(struct pollfd *fds, nfds_t nfds, int timeout, int* count) {
  *count = ki_poll(fds, nfds, timeout);
  return (*count < 0) ? errno : 0;

}

int WRAP(read)(int fd, void *buf, size_t count, size_t *nread) {
  ssize_t signed_nread = ki_read(fd, buf, count);
  *nread = static_cast<size_t>(signed_nread);
  return (signed_nread < 0) ? errno : 0;
}

int WRAP(rmdir)(const char* pathname) {
  return (ki_rmdir(pathname) < 0) ? errno : 0;
}

int WRAP(seek)(int fd, off_t offset, int whence, off_t* new_offset) {
  *new_offset = ki_lseek(fd, offset, whence);
  return (*new_offset < 0) ? errno : 0;
}

int WRAP(select)(int nfds, fd_set* readfds, fd_set* writefds,
                 fd_set* exceptfds, struct timeval* timeout, int* count) {
  *count = ki_select(nfds, readfds, writefds, exceptfds, timeout);
  return (*count < 0) ? errno : 0;
}

int WRAP(stat)(const char *pathname, struct nacl_abi_stat *nacl_buf) {
  struct stat buf;
  memset(&buf, 0, sizeof(struct stat));
  int res = ki_stat(pathname, &buf);
  if (res < 0)
    return errno;
  stat_to_nacl_stat(&buf, nacl_buf);
  return 0;
}

int WRAP(write)(int fd, const void* buf, size_t count, size_t* nwrote) {
  ssize_t signed_nwrote = ki_write(fd, buf, count);
  *nwrote = static_cast<size_t>(signed_nwrote);
  return (signed_nwrote < 0) ? errno : 0;
}

static void assign_real_pointers() {
  static bool assigned = false;
  if (!assigned) {
    EXPAND_SYMBOL_LIST_OPERATION(ASSIGN_REAL_PTR)
    assigned = true;
  }
}

#define CHECK_REAL(func) \
  if (!REAL(func)) \
    assign_real_pointers();

// "real" functions, i.e. the unwrapped original functions.

int _real_close(int fd) {
  CHECK_REAL(close);
  return REAL(close)(fd);
}

int _real_fstat(int fd, struct stat* buf) {
  struct nacl_abi_stat st;
  CHECK_REAL(fstat);
  int err = REAL(fstat)(fd, &st);
  if (err) {
    errno = err;
    return -1;
  }

  nacl_stat_to_stat(&st, buf);
  return 0;
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
    size_t d_name_len = nacl_d->nacl_abi_d_reclen -
        offsetof(nacl_abi_dirent, nacl_abi_d_name);
    memcpy(d->d_name, nacl_d->nacl_abi_d_name, d_name_len);

    offset += d->d_reclen;
    offset += nacl_d->nacl_abi_d_reclen;
  }

  *nread = offset;
  return 0;
}

int _real_lseek(int fd, off_t offset, int whence, off_t* new_offset) {
  CHECK_REAL(seek);
  return REAL(seek)(fd, offset, whence, new_offset);
}

int _real_mkdir(const char* pathname, mode_t mode) {
  CHECK_REAL(mkdir);
  return REAL(mkdir)(pathname, mode);
}

int _real_mmap(void** addr, size_t length, int prot, int flags, int fd,
               off_t offset) {
  CHECK_REAL(mmap);
  return REAL(mmap)(addr, length, prot, flags, fd, offset);
}

int _real_munmap(void* addr, size_t length) {
  CHECK_REAL(munmap);
  return REAL(munmap)(addr, length);
}

int _real_open(const char* pathname, int oflag, mode_t cmode, int* newfd) {
  CHECK_REAL(open);
  return REAL(open)(pathname, oflag, cmode, newfd);
}

int _real_open_resource(const char* file, int* fd) {
  CHECK_REAL(open_resource);
  return REAL(open_resource)(file, fd);
}

int _real_read(int fd, void *buf, size_t count, size_t *nread) {
  CHECK_REAL(read);
  return REAL(read)(fd, buf, count, nread);
}

int _real_rmdir(const char* pathname) {
  CHECK_REAL(rmdir);
  return REAL(rmdir)(pathname);
}

int _real_write(int fd, const void *buf, size_t count, size_t *nwrote) {
  CHECK_REAL(write);
  return REAL(write)(fd, buf, count, nwrote);
}

uint64_t usec_since_epoch() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_usec + (tv.tv_sec * 1000000);
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

#endif  // defined(__native_client__) && defined(__GLIBC__)
