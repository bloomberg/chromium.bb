/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef LIBRARIES_NACL_IO_KERNEL_WRAP_REAL_H_
#define LIBRARIES_NACL_IO_KERNEL_WRAP_REAL_H_

#include "nacl_io/ostypes.h"
#include "utils/macros.h"

EXTERN_C_BEGIN

int _real_close(int fd);
int _real_fstat(int fd, struct stat *buf);
int _real_getdents(int fd, void* nacl_buf, size_t nacl_count, size_t *nread);
int _real_lseek(int fd, off_t offset, int whence, off_t* new_offset);
int _real_mkdir(const char* pathname, mode_t mode);
int _real_mmap(void** addr, size_t length, int prot, int flags, int fd,
               off_t offset);
int _real_munmap(void* addr, size_t length);
int _real_open(const char* pathname, int oflag, mode_t cmode, int* newfd);
int _real_open_resource(const char* file, int* fd);
int _real_read(int fd, void *buf, size_t count, size_t *nread);
int _real_rmdir(const char* pathname);
int _real_write(int fd, const void *buf, size_t count, size_t *nwrote);

EXTERN_C_END

#endif  // LIBRARIES_NACL_IO_KERNEL_WRAP_REAL_H_
