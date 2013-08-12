// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>  // Include something that will define __GLIBC__.

// The entire file is wrapped in this #if. We do this so this .cc file can be
// compiled, even on a non-newlib build.
#if defined(__native_client__) && !defined(__GLIBC__)

#include "nacl_io/kernel_wrap.h"
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <irt.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "nacl_io/kernel_intercept.h"

EXTERN_C_BEGIN

// Macro to get the REAL function pointer
#define REAL(name) __nacl_irt_##name##_real

// Macro to get the WRAP function
#define WRAP(name) __nacl_irt_##name##_wrap

// Declare REAL function pointer.
#define DECLARE_REAL_PTR(group, name) \
  typeof(__libnacl_irt_##group.name) REAL(name);

// Assign the REAL function pointer.
#define ASSIGN_REAL_PTR(group, name) \
  assert(__libnacl_irt_##group.name != NULL); \
  REAL(name) = __libnacl_irt_##group.name;

// Switch IRT's pointer to the REAL pointer
#define USE_REAL(group, name) \
  __libnacl_irt_##group.name = (typeof(REAL(name))) REAL(name); \

// Switch the IRT's pointer to the WRAP function
#define USE_WRAP(group, name) \
  __libnacl_irt_##group.name = (typeof(REAL(name))) WRAP(name); \

extern void __libnacl_irt_filename_init(void);

extern struct nacl_irt_fdio __libnacl_irt_fdio;
extern struct nacl_irt_dev_filename __libnacl_irt_dev_filename;
extern struct nacl_irt_memory __libnacl_irt_memory;

// Create function pointers to the REAL implementation
#define EXPAND_SYMBOL_LIST_OPERATION(OP) \
  OP(fdio, close); \
  OP(fdio, dup); \
  OP(fdio, dup2); \
  OP(fdio, fstat); \
  OP(fdio, getdents); \
  OP(fdio, read); \
  OP(fdio, seek); \
  OP(fdio, write); \
  OP(dev_filename, open); \
  OP(dev_filename, stat); \
  OP(memory, mmap); \
  OP(memory, munmap);


EXPAND_SYMBOL_LIST_OPERATION(DECLARE_REAL_PTR);

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

int WRAP(fstat)(int fd, struct stat* buf) {
  return (ki_fstat(fd, buf) < 0) ? errno : 0;
}

int WRAP(getdents)(int fd, dirent* buf, size_t count, size_t* nread) {
  return (ki_getdents(fd, buf, count) < 0) ? errno : 0;
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

int WRAP(read)(int fd, void* buf, size_t count, size_t* nread) {
  if (!ki_is_initialized())
    return REAL(read)(fd, buf, count, nread);

  ssize_t signed_nread = ki_read(fd, buf, count);
  *nread = static_cast<size_t>(signed_nread);
  return (signed_nread < 0) ? errno : 0;
}

int WRAP(seek)(int fd, off_t offset, int whence, off_t* new_offset) {
  *new_offset = ki_lseek(fd, offset, whence);
  return (*new_offset < 0) ? errno : 0;
}

int WRAP(stat)(const char* pathname, struct stat* buf) {
  return (ki_stat(pathname, buf) < 0) ? errno : 0;
}

int WRAP(write)(int fd, const void* buf, size_t count, size_t* nwrote) {
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

int _real_fstat(int fd, struct stat* buf) {
  return REAL(fstat)(fd, buf);
}

int _real_getdents(int fd, dirent* nacl_buf, size_t nacl_count, size_t* nread) {
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

int _real_read(int fd, void* buf, size_t count, size_t* nread) {
  return REAL(read)(fd, buf, count, nread);
}

int _real_rmdir(const char* pathname) {
  return ENOSYS;
}

int _real_write(int fd, const void* buf, size_t count, size_t* nwrote) {
  return REAL(write)(fd, buf, count, nwrote);
}

uint64_t usec_since_epoch() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_usec + (tv.tv_sec * 1000000);
}

static bool s_wrapped = false;
static bool s_assigned = false;
void kernel_wrap_init() {
  if (!s_wrapped) {
    if (!s_assigned) {
      __libnacl_irt_filename_init();
      EXPAND_SYMBOL_LIST_OPERATION(ASSIGN_REAL_PTR)
      s_assigned = true;
    }
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


#endif  // defined(__native_client__) && !defined(__GLIBC__)

