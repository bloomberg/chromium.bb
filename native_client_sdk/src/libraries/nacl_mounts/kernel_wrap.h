/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef LIBRARIES_NACL_MOUNTS_KERNEL_WRAP_H_
#define LIBRARIES_NACL_MOUNTS_KERNEL_WRAP_H_

#include <sys/types.h>
#include <stdlib.h>
#include "utils/macros.h"

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

int NAME(access)(const char* path, int amode) NOTHROW;
int NAME(chdir)(const char* path) NOTHROW;
int NAME(chmod)(const char* path, chmod_mode_t mode) NOTHROW;
int NAME(close)(int fd);
int NAME(dup)(int oldfd) NOTHROW;
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
char* NAME(getcwd)(char* buf, getcwd_size_t size) NOTHROW;
char* getwd(char* buf) NOTHROW;
int getdents(int fd, void* buf, unsigned int count) NOTHROW;
int NAME(isatty)(int fd) NOTHROW;
off_t NAME(lseek)(int fd, off_t offset, int whence) NOTHROW;
#if defined(WIN32)
int _mkdir(const char* path);
#else
int mkdir(const char* path, mode_t mode) NOTHROW;
#endif
int mount(const char* source, const char* target, const char* filesystemtype,
          unsigned long mountflags, const void* data) NOTHROW;
int NAME(open)(const char* path, int oflag, ...);
read_ssize_t NAME(read)(int fd, void* buf, size_t nbyte);
int remove(const char* path) NOTHROW;
int NAME(rmdir)(const char* path) NOTHROW;
#if defined(WIN32)
int _stat32(const char* path, struct _stat32* buf);
int _stat64(const char* path, struct _stat64* buf);
int _stat32i64(const char* path, struct _stat32i64* buf);
int _stat64i32(const char* path, struct _stat64i32* buf);
#else
int stat(const char* path, struct stat* buf) NOTHROW;
#endif
int umount(const char* path) NOTHROW;
int NAME(unlink)(const char* path) NOTHROW;
read_ssize_t NAME(write)(int fd, const void* buf, size_t nbyte);

EXTERN_C_END

#endif  // LIBRARIES_NACL_MOUNTS_KERNEL_WRAP_H_
