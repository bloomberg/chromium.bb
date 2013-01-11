/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <sys/types.h>  // Include something that will define __GLIBC__.

// The entire file is wrapped in this #if. We do this so this .cc file can be
// compiled, even on a non-newlib build.
#if defined(__native_client__) && !defined(__GLIBC__)

#include "nacl_mounts/kernel_wrap.h"
#include <dirent.h>
#include <errno.h>
#include <irt.h>
#include <sys/stat.h>
#include "nacl_mounts/kernel_intercept.h"

EXTERN_C_BEGIN

#define REAL(name) __nacl_irt_##name##_real
#define WRAP(name) __nacl_irt_##name##_wrap
#define STRUCT_NAME(group) __libnacl_irt_##group
#define DECLARE_STRUCT(group) \
  extern struct nacl_irt_##group STRUCT_NAME(group);
#define MUX(group, name) STRUCT_NAME(group).name
#define DECLARE(group, name) typeof(MUX(group, name)) REAL(name);
#define DO_WRAP(group, name) do { \
    REAL(name) = MUX(group, name); \
    MUX(group, name) = (typeof(REAL(name))) WRAP(name); \
  } while (0)

DECLARE_STRUCT(fdio)
DECLARE_STRUCT(filename)

DECLARE(fdio, close)
DECLARE(fdio, dup)
DECLARE(fdio, fstat)
DECLARE(fdio, getdents)
DECLARE(fdio, read)
DECLARE(fdio, seek)
DECLARE(fdio, write)
DECLARE(filename, open)
DECLARE(filename, stat)


int access(const char* path, int amode) {
  return ki_access(path, amode);
}

int chdir(const char* path) {
  return ki_chdir(path);
}

int chmod(const char* path, mode_t mode) {
  return ki_chmod(path, mode);
}

int WRAP(close)(int fd) {
  return (ki_close(fd) < 0) ? errno : 0;
}

int WRAP(dup)(int fd, int* newfd) {
  *newfd = ki_dup(fd);
  return (*newfd < 0) ? errno : 0;
}

int WRAP(fstat)(int fd, struct stat *buf) {
  return (ki_fstat(fd, buf) < 0) ? errno : 0;
}

int fsync(int fd) {
  return ki_fsync(fd);
}

char* getcwd(char* buf, size_t size) {
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

char* getwd(char* buf) {
  return ki_getwd(buf);
}

int getdents(int fd, void* buf, unsigned int count) {
  return ki_getdents(fd, buf, count);
}

int WRAP(getdents)(int fd, dirent* buf, size_t count, size_t *nread) {
  return (ki_getdents(fd, buf, count) < 0) ? errno : 0;
}

int isatty(int fd) {
  return ki_isatty(fd);
}

int mkdir(const char* path, mode_t mode) {
  return ki_mkdir(path, mode);
}

int mount(const char* source, const char* target, const char* filesystemtype,
          unsigned long mountflags, const void* data) {
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

int remove(const char* path) {
  return ki_remove(path);
}

int rmdir(const char* path) {
  return ki_rmdir(path);
}

int WRAP(seek)(int fd, off_t offset, int whence, off_t* new_offset) {
  *new_offset = ki_lseek(fd, offset, whence);
  return (*new_offset < 0) ? errno : 0;
}

int WRAP(stat)(const char *pathname, struct stat *buf) {
  return (ki_stat(pathname, buf) < 0) ? errno : 0;
}

int umount(const char* path) {
  return ki_umount(path);
}

int unlink(const char* path) {
  return ki_unlink(path);
}

int WRAP(write)(int fd, const void *buf, size_t count, size_t *nwrote) {
  *nwrote = ki_write(fd, buf, count);
  return (*nwrote < 0) ? errno : 0;
}


void kernel_wrap_init() {
  DO_WRAP(fdio, close);
  DO_WRAP(fdio, dup);
  DO_WRAP(fdio, fstat);
  DO_WRAP(fdio, getdents);
  DO_WRAP(fdio, read);
  DO_WRAP(fdio, seek);
  DO_WRAP(fdio, write);
  DO_WRAP(filename, open);
  DO_WRAP(filename, stat);
}


EXTERN_C_END


#endif  // defined(__native_client__) && !defined(__GLIBC__)
