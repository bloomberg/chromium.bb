// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The entire file is wrapped in this #if. We do this so this .cc file can be
// compiled, even on a non-Windows build.
#if defined(WIN32)

#include "nacl_io/kernel_wrap.h"
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>  // This must be included before <sys/stat.h>.
#include <sys/stat.h>
#include "nacl_io/kernel_intercept.h"

#include <windows.h>

namespace {

template <typename SrcStat, typename DstStat>
void CopyStat(const SrcStat* src, DstStat* dst) {
  memset(dst, 0, sizeof(DstStat));
  dst->st_dev = src->st_dev;
  dst->st_ino = src->st_ino;
  dst->st_mode = src->st_mode;
  dst->st_nlink = src->st_nlink;
  dst->st_uid = src->st_uid;
  dst->st_gid = src->st_gid;
  dst->st_rdev = src->st_rdev;
  dst->st_size = src->st_size;
  dst->st_atime = src->st_atime;
  dst->st_mtime = src->st_mtime;
  dst->st_ctime = src->st_ctime;
}

}  // namespace

EXTERN_C_BEGIN

// This needs to be included because it is defined in read.c, which we wish to
// override. Define with dummy values for now... though this seems like it will
// break ftelli64/fgetpos/fstream.
char _lookuptrailbytes[256] = {0};

int _access(const char* path, int amode) {
  return ki_access(path, amode);
}

int _chdir(const char* path) {
  return ki_chdir(path);
}

int _chmod(const char* path, mode_t mode) {
  return ki_chmod(path, mode);
}

int _close(int fd) {
  return ki_close(fd);
}

int _close_nolock(int fd) {
  return ki_close(fd);
}

int _dup(int oldfd) {
  return ki_dup(oldfd);
}

int _dup2(int oldfd, int newfd) {
  return ki_dup2(oldfd, newfd);
}

int _fstat32(int fd, struct _stat32* buf) {
  struct stat ki_buf;
  int res = ki_fstat(fd, &ki_buf);
  CopyStat(&ki_buf, buf);
  return res;
}

int _fstat64(int fd, struct _stat64* buf) {
  struct stat ki_buf;
  int res = ki_fstat(fd, &ki_buf);
  CopyStat(&ki_buf, buf);
  return res;
}

int _fstat32i64(int fd, struct _stat32i64* buf) {
  struct stat ki_buf;
  int res = ki_fstat(fd, &ki_buf);
  CopyStat(&ki_buf, buf);
  return res;
}

int _fstat64i32(int fd, struct _stat64i32* buf) {
  struct stat ki_buf;
  int res = ki_fstat(fd, &ki_buf);
  CopyStat(&ki_buf, buf);
  return res;
}

char* _getcwd(char* buf, int size) {
  return ki_getcwd(buf, size);
}

int _isatty(int fd) {
  return ki_isatty(fd);
}

off_t _lseek(int fd, off_t offset, int whence) {
  return ki_lseek(fd, offset, whence);
}

int _mkdir(const char* path) {
  return ki_mkdir(path, 0777);
}

int _open(const char* path, int oflag, ...) {
#if 0
  // TODO(binji): ki_open should use the pmode parameter. When it does, this
  // will be necessary to add in.
  va_list list;
  int pmode = 0;
  if (oflag & _O_CREAT) {
    va_start(list, oflag);
    pmode = va_arg(list, int);
    va_end(list);
  }
#endif
  return ki_open(path, oflag);
}

int _sopen(const char* path, int oflag, int shflag) {
  return ki_open(path, oflag);
}

errno_t _sopen_s(int* pfh, const char* path, int oflag, int shflag, int pmode) {
  *pfh = ki_open(path, oflag);
  return (*pfh < 0) ? errno : 0;
}

int _read(int fd, void* buf, size_t nbyte) {
  if (!ki_is_initialized())
    return 0;

  return ki_read(fd, buf, nbyte);
}

int _read_nolock(int fd, void* buf, size_t nbyte) {
  if (!ki_is_initialized())
    return 0;

  return ki_read(fd, buf, nbyte);
}

int _rmdir(const char* path) {
  return ki_rmdir(path);
}

int setenv(const char* name, const char* value, int overwrite) {
  if (0 == overwrite && NULL != getenv(name)) {
    return 0;
  }
  errno_t result = _putenv_s(name, value);
  if (result != 0) {
    errno = result;
    return -1;
  } else {
    return 0;
  }
}

int _stat32(const char* path, struct _stat32* buf) {
  struct stat ki_buf;
  int res = ki_stat(path, &ki_buf);
  CopyStat(&ki_buf, buf);
  return res;
}

int _stat64(const char* path, struct _stat64* buf) {
  struct stat ki_buf;
  int res = ki_stat(path, &ki_buf);
  CopyStat(&ki_buf, buf);
  return res;
}

int _stat64i32(const char* path, struct _stat64i32* buf) {
  struct stat ki_buf;
  int res = ki_stat(path, &ki_buf);
  CopyStat(&ki_buf, buf);
  return res;
}

int _stat32i64(const char* path, struct _stat32i64* buf) {
  struct stat ki_buf;
  int res = ki_stat(path, &ki_buf);
  CopyStat(&ki_buf, buf);
  return res;
}

int _unlink(const char* path) {
  return ki_unlink(path);
}

int _utime(const char* filename, const struct utimbuf* times) {
  return ki_utime(filename, times);
}

int _write(int fd, const void* buf, size_t nbyte) {
  if (!ki_is_initialized())
    return 0;

  return ki_write(fd, buf, nbyte);
}


// "real" functions, i.e. the unwrapped original functions. On Windows we don't
// wrap, so the real functions aren't accessible. In most cases, we just fail.

int _real_close(int fd) {
  return ENOSYS;
}

int _real_fstat(int fd, struct stat *buf) {
  return 0;
}

int _real_getdents(int fd, void* nacl_buf, size_t nacl_count, size_t *nread) {
  return ENOSYS;
}

int _real_lseek(int fd, off_t offset, int whence, off_t* new_offset) {
  return ENOSYS;
}

int _real_mkdir(const char* pathname, mode_t mode) {
  return ENOSYS;
}

int _real_mmap(void** addr, size_t length, int prot, int flags, int fd,
               off_t offset) {
  return ENOSYS;
}

int _real_munmap(void* addr, size_t length) {
  return ENOSYS;
}

int _real_open(const char* pathname, int oflag, mode_t cmode, int* newfd) {
  return ENOSYS;
}

int _real_open_resource(const char* file, int* fd) {
  return ENOSYS;
}

int _real_read(int fd, void *buf, size_t count, size_t *nread) {
  *nread = count;
  return 0;
}

int _real_rmdir(const char* pathname) {
  return ENOSYS;
}

int _real_write(int fd, const void *buf, size_t count, size_t *nwrote) {
  *nwrote = count;
  return 0;
}

#define USECS_FROM_WIN_TO_TO_UNIX_EPOCH 11644473600000LL
uint64_t usec_since_epoch() {
  FILETIME ft;
  ULARGE_INTEGER ularge;
  GetSystemTimeAsFileTime(&ft);

  ularge.LowPart = ft.dwLowDateTime;
  ularge.HighPart = ft.dwHighDateTime;

  // Truncate to usec resolution.
  return ularge.QuadPart / 10;
}

// Do nothing for Windows, we replace the library at link time.
void kernel_wrap_init() {
}

void kernel_wrap_uninit() {
}

EXTERN_C_END

#endif   // defined(WIN32)

