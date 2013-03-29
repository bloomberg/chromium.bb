/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <sys/types.h>  // Include something that will define __GLIBC__.

// The entire file is wrapped in this #if. We do this so this .cc file can be
// compiled, even on a non-newlib build.
#if defined(__native_client__) && !defined(__GLIBC__)

#include "nacl_io/kernel_wrap.h"
#include <dirent.h>
#include <errno.h>
#include <irt.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "nacl_io/kernel_intercept.h"

EXTERN_C_BEGIN

#define REAL(name) __nacl_irt_##name##_real
#define WRAP(name) __nacl_irt_##name##_wrap
#define STRUCT_NAME(group) __libnacl_irt_##group
#define DECLARE_STRUCT(group) \
  extern struct nacl_irt_##group STRUCT_NAME(group);
#define DECLARE_STRUCT_VERSION(group, version) \
  extern struct nacl_irt_##group##_##version STRUCT_NAME(group);
#define MUX(group, name) STRUCT_NAME(group).name
#define DECLARE(group, name) typeof(MUX(group, name)) REAL(name);
#define DO_WRAP(group, name) do { \
    REAL(name) = MUX(group, name); \
    MUX(group, name) = (typeof(REAL(name))) WRAP(name); \
  } while (0)

DECLARE_STRUCT(fdio)
DECLARE_STRUCT(filename)
DECLARE_STRUCT_VERSION(memory, v0_2)

DECLARE(fdio, close)
DECLARE(fdio, dup)
DECLARE(fdio, dup2)
DECLARE(fdio, fstat)
DECLARE(fdio, getdents)
DECLARE(fdio, read)
DECLARE(fdio, seek)
DECLARE(fdio, write)
DECLARE(filename, open)
DECLARE(filename, stat)
DECLARE(memory, mmap)
DECLARE(memory, munmap)


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

int WRAP(dup2)(int fd, int newfd) {
  newfd = ki_dup2(fd, newfd);
  return (newfd < 0) ? errno : 0;
}

int WRAP(fstat)(int fd, struct stat *buf) {
  return (ki_fstat(fd, buf) < 0) ? errno : 0;
}

int fsync(int fd) {
  return ki_fsync(fd);
}

char* getcwd(char* buf, size_t size) {
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

int link(const char* oldpath, const char* newpath) {
  return ki_link(oldpath, newpath);
}

int mkdir(const char* path, mode_t mode) {
  return ki_mkdir(path, mode);
}

int WRAP(mmap)(void** addr, size_t length, int prot, int flags, int fd,
               off_t offset) {
  if (flags & MAP_ANONYMOUS)
    return REAL(mmap)(addr, length, prot, flags, fd, offset);

  *addr = ki_mmap(*addr, length, prot, flags, fd, offset);
  return *addr == (void*)-1 ? errno : 0;
}

int mount(const char* source, const char* target, const char* filesystemtype,
          unsigned long mountflags, const void* data) {
  return ki_mount(source, target, filesystemtype, mountflags, data);
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

int WRAP(read)(int fd, void *buf, size_t count, size_t *nread) {
  if (!ki_is_initialized())
    return REAL(read)(fd, buf, count, nread);

  ssize_t signed_nread = ki_read(fd, buf, count);
  *nread = static_cast<size_t>(signed_nread);
  return (signed_nread < 0) ? errno : 0;
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

int symlink(const char* oldpath, const char* newpath) {
  return ki_symlink(oldpath, newpath);
}

int umount(const char* path) {
  return ki_umount(path);
}

int unlink(const char* path) {
  return ki_unlink(path);
}

int WRAP(write)(int fd, const void *buf, size_t count, size_t *nwrote) {
  if (!ki_is_initialized())
    return REAL(write)(fd, buf, count, nwrote);

  ssize_t signed_nwrote = ki_write(fd, buf, count);
  *nwrote = static_cast<size_t>(signed_nwrote);
  return (signed_nwrote < 0) ? errno : 0;
}


// "real" functions, i.e. the unwrapped original functions.

int _real_close(int fd) {
  return REAL(close)(fd);
}

int _real_fstat(int fd, struct stat *buf) {
  return REAL(fstat)(fd, buf);
}

int _real_getdents(int fd, dirent* nacl_buf, size_t nacl_count, size_t *nread) {
  return REAL(getdents)(fd, nacl_buf, nacl_count, nread);
}

int _real_lseek(int fd, off_t offset, int whence, off_t* new_offset) {
  return REAL(seek)(fd, offset, whence, new_offset);
}

int _real_mkdir(const char* pathname, mode_t mode) {
  return ENOSYS;
}

int _real_mmap(void** addr, size_t length, int prot, int flags, int fd,
               off_t offset) {
  return REAL(mmap)(addr, length, prot, flags, fd, offset);
}

int _real_munmap(void* addr, size_t length) {
  return REAL(munmap)(addr, length);
}

int _real_open(const char* pathname, int oflag, mode_t cmode, int* newfd) {
  return REAL(open)(pathname, oflag, cmode, newfd);
}

int _real_open_resource(const char* file, int* fd) {
  return ENOSYS;
}

int _real_read(int fd, void *buf, size_t count, size_t *nread) {
  return REAL(read)(fd, buf, count, nread);
}

int _real_rmdir(const char* pathname) {
  return ENOSYS;
}

int _real_write(int fd, const void *buf, size_t count, size_t *nwrote) {
  return REAL(write)(fd, buf, count, nwrote);
}


void kernel_wrap_init() {
  static bool wrapped = false;

  if (!wrapped) {
    wrapped = true;
    DO_WRAP(fdio, close);
    DO_WRAP(fdio, dup);
    DO_WRAP(fdio, dup2);
    DO_WRAP(fdio, fstat);
    DO_WRAP(fdio, getdents);
    DO_WRAP(fdio, read);
    DO_WRAP(fdio, seek);
    DO_WRAP(fdio, write);
    DO_WRAP(filename, open);
    DO_WRAP(filename, stat);
    DO_WRAP(memory, mmap);
    DO_WRAP(memory, munmap);
  }
}


EXTERN_C_END


#endif  // defined(__native_client__) && !defined(__GLIBC__)
