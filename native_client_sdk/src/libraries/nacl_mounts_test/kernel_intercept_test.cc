/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

#include <map>
#include <string>

#include "nacl_mounts/kernel_intercept.h"
#include "nacl_mounts/kernel_proxy.h"
#include "nacl_mounts/path.h"

#define __STDC__ 1
#include "gtest/gtest.h"

class KernelProxyMock : public KernelProxy {
 public:
  KernelProxyMock() {}
  virtual ~KernelProxyMock() {}

  int chdir(const char* path) {
    events.push_back("chdir");
    return 0;
  }
  char* getcwd(char* buf, size_t size) {
    events.push_back("getcwd");
    return buf;
  }
  char* getwd(char* buf) {
    events.push_back("getwd");
    return buf;
  }
  int dup(int oldfd) {
    events.push_back("dup");
    return oldfd;
  }
  int chmod(const char *path, mode_t mode) {
    events.push_back("chmod");
    return 0;
  }
  int stat(const char *path, struct stat *buf) {
    events.push_back("stat");
    return 0;
  }
  int mkdir(const char *path, mode_t mode) {
    events.push_back("mkdir");
    return 0;
  }
  int rmdir(const char *path) {
    events.push_back("rmdir");
    return 0;
  }
  int mount(const char *source, const char *target,
      const char *filesystemtype, unsigned long mountflags, const void *data) {
    events.push_back("mount");
    return 0;
  }
  int umount(const char *path) {
    events.push_back("umount");
    return 0;
  }
  int open(const char *path, int oflag) {
    events.push_back("open");
    return 0;
  }
  ssize_t read(int fd, void *buf, size_t nbyte) {
    events.push_back("read");
    return 0;
  }
  ssize_t write(int fd, const void *buf, size_t nbyte) {
    events.push_back("write");
    return 0;
  }
  int fstat(int fd, struct stat *buf) {
    events.push_back("fstat");
    return 0;
  }
  int getdents(int fd, void *buf, unsigned int count) {
    events.push_back("getdents");
    return 0;
  }
  int fsync(int fd) {
    events.push_back("fsync");
    return 0;
  }
  int isatty(int fd) {
    events.push_back("isatty");
    return 0;
  }
  int close(int fd) {
    events.push_back("close");
    return 0;
  }
  off_t lseek(int fd, off_t offset, int whence) {
    events.push_back("lseek");
    return 0;
  }
  int remove(const std::string& path) {
    events.push_back("remove");
    return 0;
  }
  int unlink(const std::string& path) {
    events.push_back("unlink");
    return 0;
  }
  int access(const std::string& path, int amode) {
    events.push_back("access");
    return 0;
  }

  std::string& LastStr() {
    return events.back();
  }

  std::vector<std::string> events;
};


TEST(KernelIntercept, SanityChecks) {
  static KernelProxyMock* mock = new KernelProxyMock();
  ki_init(mock);

  ki_chdir("foo");
  EXPECT_EQ("chdir", mock->LastStr());

  ki_getcwd("foo", 1);
  EXPECT_EQ("getcwd", mock->LastStr());

  ki_getwd("foo");
  EXPECT_EQ("getwd", mock->LastStr());

  ki_dup(1);
  EXPECT_EQ("dup", mock->LastStr());

  ki_chmod("foo", NULL);
  EXPECT_EQ("chmod", mock->LastStr());

  ki_stat("foo", NULL);
  EXPECT_EQ("stat", mock->LastStr());

  ki_mkdir("foo", 0);
  EXPECT_EQ("mkdir", mock->LastStr());

  ki_rmdir("foo");
  EXPECT_EQ("rmdir", mock->LastStr());

  ki_mount("foo", "bar", NULL, 0, NULL);
  EXPECT_EQ("mount", mock->LastStr());

  ki_umount("foo");
  EXPECT_EQ("umount", mock->LastStr());

  ki_open("foo", 0);
  EXPECT_EQ("open", mock->LastStr());

  ki_read(1, NULL, 0);
  EXPECT_EQ("read", mock->LastStr());

  ki_write(1, NULL, 0);
  EXPECT_EQ("write", mock->LastStr());

  ki_fstat(1, NULL);
  EXPECT_EQ("fstat", mock->LastStr());

  ki_getdents(1, NULL, 0);
  EXPECT_EQ("getdents", mock->LastStr());

  ki_fsync(1);
  EXPECT_EQ("fsync", mock->LastStr());

  ki_isatty(1);
  EXPECT_EQ("isatty", mock->LastStr());

  ki_close(1);
  EXPECT_EQ("close", mock->LastStr());
}

