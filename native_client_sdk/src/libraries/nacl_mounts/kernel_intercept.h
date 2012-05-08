/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef LIBRARIES_NACL_MOUNT_KERNEL_INTERCEPT_H_
#define LIBRARIES_NACL_MOUNT_KERNEL_INTERCEPT_H_

#include <stdint.h>

#include "utils/macros.h"

EXTERN_C_BEGIN

// The kernel intercept module provides a C->C++ thunk between the libc
// kernel calls and the KernelProxy singleton.

// ki_init must be called with an uninitialized KernelProxy object.  Calling
// with NULL will instantiate a default kernel proxy object.  ki_init must
// be called before any other ki_XXX function can be used.
void ki_init(void* kernel_proxy);

int ki_chdir(const char* path);
char* ki_getcwd(char* buf, size_t size);
char* ki_getwd(char* buf);
int ki_dup(int oldfd);
int ki_chmod(const char* path, mode_t mode);
int ki_stat(const char* path, struct stat* buf);
int ki_mkdir(const char* path, mode_t mode);
int ki_rmdir(const char* path);
int ki_mount(const char* source, const char* target, const char* filesystemtype,
             unsigned long mountflags, const void *data);
int ki_umount(const char* path);
int ki_open(const char* path, int oflag);
ssize_t ki_read(int fd, void* buf, size_t nbyte);
ssize_t ki_write(int fd, const void* buf, size_t nbyte);
int ki_fstat(int fd, struct stat *buf);
int ki_getdents(int fd, void* buf, unsigned int count);
int ki_fsync(int fd);
int ki_isatty(int fd);
int ki_close(int fd);

EXTERN_C_END

#endif  // LIBRARIES_NACL_MOUNT_KERNEL_INTERCEPT_H_
