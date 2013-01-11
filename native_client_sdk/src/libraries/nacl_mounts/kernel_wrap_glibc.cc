/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <sys/types.h>  // Include something that will define __GLIBC__.

// The entire file is wrapped in this #if. We do this so this .cc file can be
// compiled, even on a non-glibc build.
#if defined(__native_client__) && defined(__GLIBC__)

#include "nacl_mounts/kernel_wrap.h"
#include <alloca.h>
#include <dirent.h>
#include <errno.h>
#include <irt.h>
#include <irt_syscalls.h>
#include <nacl_stat.h>
#include <string.h>
#include <sys/stat.h>
#include "nacl_mounts/kernel_intercept.h"


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

#define REAL(name) __nacl_irt_##name##_real
#define WRAP(name) __nacl_irt_##name##_wrap
#define MUX(name) __nacl_irt_##name
#define DECLARE(name) typeof(MUX(name)) REAL(name);
#define DO_WRAP(name) do { \
    REAL(name) = MUX(name); \
    MUX(name) = (typeof(REAL(name))) WRAP(name); \
  } while (0)

DECLARE(chdir)
DECLARE(close)
DECLARE(dup)
DECLARE(fstat)
DECLARE(getcwd)
DECLARE(getdents)
DECLARE(mkdir)
DECLARE(open)
DECLARE(read)
DECLARE(rmdir)
DECLARE(seek)
DECLARE(stat)
DECLARE(write)

int access(const char* path, int amode) NOTHROW {
  return ki_access(path, amode);
}

int chdir(const char* path) NOTHROW {
  return ki_chdir(path);
}

int chmod(const char* path, mode_t mode) NOTHROW {
  return ki_chmod(path, mode);
}

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

int WRAP(fstat)(int fd, struct nacl_abi_stat *nacl_buf) {
  struct stat buf;
  memset(&buf, 0, sizeof(struct stat));
  int res = ki_fstat(fd, &buf);
  if (res < 0)
    return errno;
  stat_to_nacl_stat(&buf, nacl_buf);
  return 0;
}

int fsync(int fd) {
  return ki_fsync(fd);
}

char* getcwd(char* buf, size_t size) NOTHROW {
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
  return ki_getcwd(buf, size);
}

char* WRAP(getcwd)(char* buf, size_t size) {
  return ki_getcwd(buf, size);
}

char* getwd(char* buf) NOTHROW {
  return ki_getwd(buf);
}

int getdents(int fd, void* buf, unsigned int count) NOTHROW {
  return ki_getdents(fd, buf, count);
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

int isatty(int fd) NOTHROW {
  return ki_isatty(fd);
}

int mkdir(const char* path, mode_t mode) NOTHROW {
  return ki_mkdir(path, mode);
}

int WRAP(mkdir) (const char* pathname, mode_t mode) {
  return (ki_mkdir(pathname, mode)) ? errno : 0;
}

int mount(const char* source, const char* target, const char* filesystemtype,
          unsigned long mountflags, const void* data) NOTHROW {
  return ki_mount(source, target, filesystemtype, mountflags, data);
}

int WRAP(open)(const char* pathname, int oflag, mode_t cmode, int* newfd) {
  *newfd = ki_open(pathname, oflag);
  return (*newfd < 0) ? errno : 0;
}

int WRAP(read)(int fd, void *buf, size_t count, size_t *nread) {
  *nread = ki_read(fd, buf, count);
  return (*nread < 0) ? errno : 0;
}

int remove(const char* path) NOTHROW {
  return ki_remove(path);
}

int rmdir(const char* path) NOTHROW {
  return ki_rmdir(path);
}

int WRAP(rmdir)(const char* pathname) {
  return (ki_rmdir(pathname) < 0) ? errno : 0;
}

int WRAP(seek)(int fd, off_t offset, int whence, off_t* new_offset) {
  *new_offset = ki_lseek(fd, offset, whence);
  return (*new_offset < 0) ? errno : 0;
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

int umount(const char* path) NOTHROW {
  return ki_umount(path);
}

int unlink(const char* path) NOTHROW {
  return ki_unlink(path);
}

int WRAP(write)(int fd, const void *buf, size_t count, size_t *nwrote) {
  *nwrote = ki_write(fd, buf, count);
  return (*nwrote < 0) ? errno : 0;
}

void kernel_wrap_init() {
  DO_WRAP(chdir);
  DO_WRAP(close);
  DO_WRAP(dup);
  DO_WRAP(fstat);
  DO_WRAP(getcwd);
  DO_WRAP(getdents);
  DO_WRAP(mkdir);
  DO_WRAP(open);
  DO_WRAP(read);
  DO_WRAP(rmdir);
  DO_WRAP(seek);
  DO_WRAP(stat);
  DO_WRAP(write);
}

EXTERN_C_END


#endif  // defined(__native_client__) && defined(__GLIBC__)
