/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef LIBRARIES_NACL_IO_TEST_KERNEL_PROXY_MOCK_H_
#define LIBRARIES_NACL_IO_TEST_KERNEL_PROXY_MOCK_H_

#include <sys/types.h>
#include <sys/stat.h>
#include "gmock/gmock.h"

#include "nacl_io/kernel_proxy.h"

class KernelProxyMock : public KernelProxy {
 public:
  KernelProxyMock();
  virtual ~KernelProxyMock();

  MOCK_METHOD2(access, int(const char*, int));
  MOCK_METHOD1(chdir, int(const char*));
  MOCK_METHOD2(chmod, int(const char*, mode_t));
  MOCK_METHOD1(close, int(int));
  MOCK_METHOD1(dup, int(int));
  MOCK_METHOD2(dup2, int(int, int));
  MOCK_METHOD2(fstat, int(int, struct stat*));
  MOCK_METHOD1(fsync, int(int));
  MOCK_METHOD2(getcwd, char*(char*, size_t));
  MOCK_METHOD3(getdents, int(int, void*, unsigned int));
  MOCK_METHOD1(getwd, char*(char*));
  MOCK_METHOD1(isatty, int(int));
  MOCK_METHOD3(lseek, off_t(int, off_t, int));
  MOCK_METHOD2(mkdir, int(const char*, mode_t));
  MOCK_METHOD5(mount, int(const char*, const char*, const char*, unsigned long,
                          const void*));
  MOCK_METHOD2(open, int(const char*, int));
  MOCK_METHOD3(read, ssize_t(int, void*, size_t));
  MOCK_METHOD1(remove, int(const char*));
  MOCK_METHOD1(rmdir, int(const char*));
  MOCK_METHOD2(stat, int(const char*, struct stat*));
  MOCK_METHOD1(umount, int(const char*));
  MOCK_METHOD1(unlink, int(const char*));
  MOCK_METHOD3(write, ssize_t(int, const void*, size_t));
  MOCK_METHOD2(link, int(const char*, const char*));
  MOCK_METHOD2(symlink, int(const char*, const char*));
  MOCK_METHOD6(mmap, void*(void*, size_t, int, int, int, size_t));
  MOCK_METHOD1(open_resource, int(const char*));
};

#endif  // LIBRARIES_NACL_IO_TEST_KERNEL_PROXY_MOCK_H_
