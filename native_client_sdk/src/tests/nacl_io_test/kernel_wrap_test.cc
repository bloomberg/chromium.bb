// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>

#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "kernel_proxy_mock.h"
#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_wrap.h"
#include "nacl_io/osmman.h"
#include "nacl_io/ossocket.h"
#include "nacl_io/ostermios.h"

#if defined(__native_client__) && !defined(__GLIBC__)
extern "C" {
// TODO(sbc): remove once these get added to the newlib toolchain headers.
int fchdir(int fd);
int utimes(const char *filename, const struct timeval times[2]);
}
#endif

using namespace nacl_io;

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::StrEq;

namespace {

#define COMPARE_FIELD(f)                                                     \
  if (arg->f != statbuf->f) {                                                \
    *result_listener << "mismatch of field \"" #f                            \
                        "\". "                                               \
                        "expected: " << statbuf->f << " actual: " << arg->f; \
    return false;                                                            \
  }

MATCHER_P(IsEqualToStatbuf, statbuf, "") {
  COMPARE_FIELD(st_dev);
  COMPARE_FIELD(st_ino);
  COMPARE_FIELD(st_mode);
  COMPARE_FIELD(st_nlink);
  COMPARE_FIELD(st_uid);
  COMPARE_FIELD(st_gid);
  COMPARE_FIELD(st_rdev);
  COMPARE_FIELD(st_size);
  COMPARE_FIELD(st_atime);
  COMPARE_FIELD(st_mtime);
  COMPARE_FIELD(st_ctime);
  return true;
}

#undef COMPARE_FIELD

ACTION_P(SetStat, statbuf) {
  memset(arg1, 0, sizeof(struct stat));
  arg1->st_dev = statbuf->st_dev;
  arg1->st_ino = statbuf->st_ino;
  arg1->st_mode = statbuf->st_mode;
  arg1->st_nlink = statbuf->st_nlink;
  arg1->st_uid = statbuf->st_uid;
  arg1->st_gid = statbuf->st_gid;
  arg1->st_rdev = statbuf->st_rdev;
  arg1->st_size = statbuf->st_size;
  arg1->st_atime = statbuf->st_atime;
  arg1->st_mtime = statbuf->st_mtime;
  arg1->st_ctime = statbuf->st_ctime;
}

void MakeDummyStatbuf(struct stat* statbuf) {
  memset(&statbuf[0], 0, sizeof(struct stat));
  statbuf->st_dev = 1;
  statbuf->st_ino = 2;
  statbuf->st_mode = 3;
  statbuf->st_nlink = 4;
  statbuf->st_uid = 5;
  statbuf->st_gid = 6;
  statbuf->st_rdev = 7;
  statbuf->st_size = 8;
  statbuf->st_atime = 9;
  statbuf->st_mtime = 10;
  statbuf->st_ctime = 11;
}

const int kDummyInt = 0xdedbeef;
const int kDummyInt2 = 0xcabba6e;
const int kDummyInt3 = 0xf00ba4;
const int kDummyInt4 = 0xabacdba;
const size_t kDummySizeT = 0x60067e;
const char* kDummyConstChar = "foobar";
const char* kDummyConstChar2 = "g00gl3";
const char* kDummyConstChar3 = "fr00gl3";
const void* kDummyVoidPtr = "blahblah";
const uid_t kDummyUid = 1001;
const gid_t kDummyGid = 1002;

class KernelWrapTest : public ::testing::Test {
 public:
  KernelWrapTest() {}

  virtual void SetUp() {
    // Initializing the KernelProxy opens stdin/stdout/stderr.
    EXPECT_CALL(mock, open(_, _))
        .WillOnce(Return(0))
        .WillOnce(Return(1))
        .WillOnce(Return(2));
    // And will call mount / and /dev.
    EXPECT_CALL(mock, mount(_, _, _, _, _))
        .WillOnce(Return(0))
        .WillOnce(Return(0));

    ki_init(&mock);
  }

  virtual void TearDown() { ki_uninit(); }

  KernelProxyMock mock;
};

}  // namespace

TEST_F(KernelWrapTest, access) {
  EXPECT_CALL(mock, access(kDummyConstChar, kDummyInt)) .WillOnce(Return(-1));
  EXPECT_EQ(-1, access(kDummyConstChar, kDummyInt));

  EXPECT_CALL(mock, access(kDummyConstChar, kDummyInt)) .WillOnce(Return(0));
  EXPECT_EQ(0, access(kDummyConstChar, kDummyInt));
}

TEST_F(KernelWrapTest, chdir) {
  EXPECT_CALL(mock, chdir(kDummyConstChar)).WillOnce(Return(-1));
  EXPECT_EQ(-1, chdir(kDummyConstChar));

  EXPECT_CALL(mock, chdir(kDummyConstChar)).WillOnce(Return(0));
  EXPECT_EQ(0, chdir(kDummyConstChar));
}

TEST_F(KernelWrapTest, chmod) {
  EXPECT_CALL(mock, chmod(kDummyConstChar, kDummyInt))
      .WillOnce(Return(kDummyInt2));
  EXPECT_EQ(kDummyInt2, chmod(kDummyConstChar, kDummyInt));
}

TEST_F(KernelWrapTest, chown) {
  EXPECT_CALL(mock, chown(kDummyConstChar, kDummyUid, kDummyGid))
      .WillOnce(Return(kDummyInt));
  EXPECT_EQ(kDummyInt, chown(kDummyConstChar, kDummyUid, kDummyGid));
}

TEST_F(KernelWrapTest, close) {
  // The way we wrap close does not support returning arbitrary values, so we
  // test 0 and -1.
  EXPECT_CALL(mock, close(kDummyInt))
      .WillOnce(Return(0))
      .WillOnce(Return(-1));

  EXPECT_EQ(0, close(kDummyInt));
  EXPECT_EQ(-1, close(kDummyInt));
}

TEST_F(KernelWrapTest, dup) {
  EXPECT_CALL(mock, dup(kDummyInt)).WillOnce(Return(kDummyInt2));
  EXPECT_EQ(kDummyInt2, dup(kDummyInt));
}

TEST_F(KernelWrapTest, dup2) {
  // The way we wrap dup2 does not support returning aribtrary values, only -1
  // or the value of the new fd.
  EXPECT_CALL(mock, dup2(kDummyInt, kDummyInt2))
      .WillOnce(Return(kDummyInt2))
      .WillOnce(Return(-1));
  EXPECT_EQ(kDummyInt2, dup2(kDummyInt, kDummyInt2));
  EXPECT_EQ(-1, dup2(kDummyInt, kDummyInt2));
}

TEST_F(KernelWrapTest, fchdir) {
  EXPECT_CALL(mock, fchdir(kDummyInt))
      .WillOnce(Return(-1));
  EXPECT_EQ(-1, fchdir(kDummyInt));
}

TEST_F(KernelWrapTest, fchmod) {
  EXPECT_CALL(mock, fchmod(kDummyInt, kDummyInt2)) .WillOnce(Return(-1));
  EXPECT_EQ(-1, fchmod(kDummyInt, kDummyInt2));

  EXPECT_CALL(mock, fchmod(kDummyInt, kDummyInt2)) .WillOnce(Return(0));
  EXPECT_EQ(0, fchmod(kDummyInt, kDummyInt2));
}

TEST_F(KernelWrapTest, fchown) {
  EXPECT_CALL(mock, fchown(kDummyInt, kDummyUid, kDummyGid))
      .WillOnce(Return(kDummyInt));
  EXPECT_EQ(kDummyInt, fchown(kDummyInt, kDummyUid, kDummyGid));
}

TEST_F(KernelWrapTest, fcntl) {
  char buffer[] = "fcntl";
  EXPECT_CALL(mock, fcntl(kDummyInt, kDummyInt2, _))
      .WillOnce(Return(kDummyInt3));
  EXPECT_EQ(kDummyInt3, fcntl(kDummyInt, kDummyInt2, buffer));
}

TEST_F(KernelWrapTest, fdatasync) {
  EXPECT_CALL(mock, fdatasync(kDummyInt)).WillOnce(Return(-1));
  EXPECT_EQ(-1, fdatasync(kDummyInt));

  EXPECT_CALL(mock, fdatasync(kDummyInt)).WillOnce(Return(0));
  EXPECT_EQ(0, fdatasync(kDummyInt));
}

TEST_F(KernelWrapTest, fstat) {
  // The way we wrap fstat does not support returning aribtrary values, only 0
  // or -1.
  struct stat in_statbuf;
  MakeDummyStatbuf(&in_statbuf);
  EXPECT_CALL(mock, fstat(kDummyInt, _))
      .WillOnce(DoAll(SetStat(&in_statbuf), Return(0)))
      .WillOnce(Return(-1));
  struct stat out_statbuf;
  EXPECT_EQ(0, fstat(kDummyInt, &out_statbuf));
  EXPECT_THAT(&in_statbuf, IsEqualToStatbuf(&out_statbuf));
  EXPECT_EQ(-1, fstat(kDummyInt, &out_statbuf));
}

TEST_F(KernelWrapTest, ftruncate) {
  EXPECT_CALL(mock, ftruncate(kDummyInt, kDummyInt2))
      .WillOnce(Return(kDummyInt3));
  EXPECT_EQ(kDummyInt3, ftruncate(kDummyInt, kDummyInt2));
}

TEST_F(KernelWrapTest, fsync) {
  EXPECT_CALL(mock, fsync(kDummyInt)).WillOnce(Return(-1));
  EXPECT_EQ(-1, fsync(kDummyInt));
}

TEST_F(KernelWrapTest, getcwd) {
  char result[] = "getcwd_result";
  char buffer[] = "getcwd";
  EXPECT_CALL(mock, getcwd(buffer, kDummySizeT)).WillOnce(Return(result));
  EXPECT_EQ(result, getcwd(buffer, kDummySizeT));
}

TEST_F(KernelWrapTest, getdents) {
#ifndef __GLIBC__
  // TODO(sbc): Find a way to test the getdents wrapper under glibc.
  // It looks like the only way to exercise it is to call readdir(2).
  // There is an internal glibc function __getdents that will call the
  // IRT but that cannot be accessed from here as glibc does not export it.
  int dummy_val;
  void* void_ptr = &dummy_val;
  EXPECT_CALL(mock, getdents(kDummyInt, void_ptr, kDummyInt2))
      .WillOnce(Return(kDummyInt2));
  EXPECT_EQ(kDummyInt2, getdents(kDummyInt, void_ptr, kDummyInt2));
#endif
}

// gcc gives error: getwd is deprecated.
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
TEST_F(KernelWrapTest, getwd) {
  char result[] = "getwd_result";
  char buffer[] = "getwd";
  EXPECT_CALL(mock, getwd(buffer)).WillOnce(Return(result));
  EXPECT_EQ(result, getwd(buffer));
}
#if defined(__GNUC__)
#pragma GCC diagnostic warning "-Wdeprecated-declarations"
#endif

TEST_F(KernelWrapTest, ioctl) {
  char buffer[] = "ioctl";
  EXPECT_CALL(mock, ioctl(kDummyInt, kDummyInt2, _))
      .WillOnce(Return(kDummyInt3));
  EXPECT_EQ(kDummyInt3, ioctl(kDummyInt, kDummyInt2, buffer));
}

TEST_F(KernelWrapTest, isatty) {
  EXPECT_CALL(mock, isatty(kDummyInt)).WillOnce(Return(kDummyInt2));
  EXPECT_EQ(kDummyInt2, isatty(kDummyInt));
}

TEST_F(KernelWrapTest, kill) {
  EXPECT_CALL(mock, kill(kDummyInt, kDummyInt2)).WillOnce(Return(kDummyInt3));
  EXPECT_EQ(kDummyInt3, kill(kDummyInt, kDummyInt2));
}

TEST_F(KernelWrapTest, lchown) {
  EXPECT_CALL(mock, lchown(kDummyConstChar, kDummyUid, kDummyGid))
      .WillOnce(Return(kDummyInt));
  EXPECT_EQ(kDummyInt, lchown(kDummyConstChar, kDummyUid, kDummyGid));
}

TEST_F(KernelWrapTest, link) {
  EXPECT_CALL(mock, link(kDummyConstChar, kDummyConstChar2))
      .WillOnce(Return(kDummyInt));
  EXPECT_EQ(kDummyInt, link(kDummyConstChar, kDummyConstChar2));
}

TEST_F(KernelWrapTest, lseek) {
  EXPECT_CALL(mock, lseek(kDummyInt, kDummyInt2, kDummyInt3))
      .WillOnce(Return(kDummyInt4));
  EXPECT_EQ(kDummyInt4, lseek(kDummyInt, kDummyInt2, kDummyInt3));
}

TEST_F(KernelWrapTest, mkdir) {
#if defined(WIN32)
  EXPECT_CALL(mock, mkdir(kDummyConstChar, 0777)).WillOnce(Return(kDummyInt2));
  EXPECT_EQ(kDummyInt2, mkdir(kDummyConstChar));
#else
  EXPECT_CALL(mock, mkdir(kDummyConstChar, kDummyInt))
      .WillOnce(Return(kDummyInt2));
  EXPECT_EQ(kDummyInt2, mkdir(kDummyConstChar, kDummyInt));
#endif
}

TEST_F(KernelWrapTest, mmap) {
  // We only wrap mmap if |flags| has the MAP_ANONYMOUS bit unset.
  int flags = kDummyInt2 & ~MAP_ANONYMOUS;

  const size_t kDummySizeT2 = 0xbadf00d;
  int dummy1 = 123;
  int dummy2 = 456;
  void* kDummyVoidPtr1 = &dummy1;
  void* kDummyVoidPtr2 = &dummy2;
  EXPECT_CALL(mock,
              mmap(kDummyVoidPtr1,
                   kDummySizeT,
                   kDummyInt,
                   flags,
                   kDummyInt3,
                   kDummySizeT2)).WillOnce(Return(kDummyVoidPtr2));
  EXPECT_EQ(kDummyVoidPtr2,
            mmap(kDummyVoidPtr1,
                 kDummySizeT,
                 kDummyInt,
                 flags,
                 kDummyInt3,
                 kDummySizeT2));
}

TEST_F(KernelWrapTest, mount) {
  EXPECT_CALL(mock,
              mount(kDummyConstChar,
                    kDummyConstChar2,
                    kDummyConstChar3,
                    kDummyInt,
                    kDummyVoidPtr)).WillOnce(Return(kDummyInt2));
  EXPECT_EQ(kDummyInt2,
            mount(kDummyConstChar,
                  kDummyConstChar2,
                  kDummyConstChar3,
                  kDummyInt,
                  kDummyVoidPtr));
}

TEST_F(KernelWrapTest, munmap) {
  // The way we wrap munmap, calls the "real" mmap as well as the intercepted
  // one. The result returned is from the "real" mmap.
  int dummy1 = 123;
  void* kDummyVoidPtr = &dummy1;
  size_t kDummySizeT = sizeof(kDummyVoidPtr);
  EXPECT_CALL(mock, munmap(kDummyVoidPtr, kDummySizeT));
  munmap(kDummyVoidPtr, kDummySizeT);
}


TEST_F(KernelWrapTest, open) {
  EXPECT_CALL(mock, open(kDummyConstChar, kDummyInt))
      .WillOnce(Return(kDummyInt2));
  EXPECT_EQ(kDummyInt2, open(kDummyConstChar, kDummyInt));

  EXPECT_CALL(mock, open(kDummyConstChar, kDummyInt))
      .WillOnce(Return(kDummyInt2));
  EXPECT_EQ(kDummyInt2, open(kDummyConstChar, kDummyInt));
}

TEST_F(KernelWrapTest, pipe) {
  int fds[] = {1, 2};
  EXPECT_CALL(mock, pipe(fds)).WillOnce(Return(kDummyInt));
  EXPECT_EQ(kDummyInt, pipe(fds));
}

TEST_F(KernelWrapTest, read) {
  int dummy_value;
  void* dummy_void_ptr = &dummy_value;
  EXPECT_CALL(mock, read(kDummyInt, dummy_void_ptr, kDummyInt2))
      .WillOnce(Return(kDummyInt3));
  EXPECT_EQ(kDummyInt3, read(kDummyInt, dummy_void_ptr, kDummyInt2));
}

TEST_F(KernelWrapTest, readlink) {
  char buf[10];

  EXPECT_CALL(mock, readlink(kDummyConstChar, buf, 10))
      .WillOnce(Return(-1));
  EXPECT_EQ(-1, readlink(kDummyConstChar, buf, 10));

  EXPECT_CALL(mock, readlink(kDummyConstChar, buf, 10))
      .WillOnce(Return(kDummyInt));
  EXPECT_EQ(kDummyInt, readlink(kDummyConstChar, buf, 10));
}

#ifdef __GLIBC__
// Under newlib there is no remove syscall.  Instead it is implemented
// in terms of unlink()/rmdir().
TEST_F(KernelWrapTest, remove) {
  EXPECT_CALL(mock, remove(kDummyConstChar)).WillOnce(Return(-1));
  EXPECT_EQ(-1, remove(kDummyConstChar));
}
#endif

TEST_F(KernelWrapTest, rename) {
  EXPECT_CALL(mock, rename(kDummyConstChar, kDummyConstChar2))
      .WillOnce(Return(-1));
  EXPECT_EQ(-1, rename(kDummyConstChar, kDummyConstChar2));

  EXPECT_CALL(mock, rename(kDummyConstChar, kDummyConstChar2))
      .WillOnce(Return(0));
  EXPECT_EQ(0, rename(kDummyConstChar, kDummyConstChar2));
}

TEST_F(KernelWrapTest, rmdir) {
  EXPECT_CALL(mock, rmdir(kDummyConstChar)).WillOnce(Return(kDummyInt));
  EXPECT_EQ(kDummyInt, rmdir(kDummyConstChar));
}

static void new_handler(int) {}
static void old_handler(int) {}

TEST_F(KernelWrapTest, sigset) {
  EXPECT_CALL(mock, sigset(kDummyInt, new_handler))
      .WillOnce(Return(old_handler));
  EXPECT_EQ(&old_handler, sigset(kDummyInt, new_handler));
}

TEST_F(KernelWrapTest, signal) {
  // KernelIntercept forwards calls to signal to KernelProxy::sigset.
  EXPECT_CALL(mock, sigset(kDummyInt, new_handler))
      .WillOnce(Return(old_handler));
  EXPECT_EQ(&old_handler, signal(kDummyInt, new_handler));
}

TEST_F(KernelWrapTest, stat) {
  // The way we wrap stat does not support returning aribtrary values, only 0
  // or -1.
  struct stat in_statbuf;
  MakeDummyStatbuf(&in_statbuf);
  EXPECT_CALL(mock, stat(StrEq(kDummyConstChar), _))
      .WillOnce(DoAll(SetStat(&in_statbuf), Return(0)))
      .WillOnce(Return(-1));
  struct stat out_statbuf;
  EXPECT_EQ(0, stat(kDummyConstChar, &out_statbuf));
  EXPECT_THAT(&in_statbuf, IsEqualToStatbuf(&out_statbuf));
  EXPECT_EQ(-1, stat(kDummyConstChar, &out_statbuf));
}

TEST_F(KernelWrapTest, symlink) {
  EXPECT_CALL(mock, symlink(kDummyConstChar, kDummyConstChar2))
      .WillOnce(Return(kDummyInt));
  EXPECT_EQ(kDummyInt, symlink(kDummyConstChar, kDummyConstChar2));
}

TEST_F(KernelWrapTest, tcflush) {
  EXPECT_CALL(mock, tcflush(kDummyInt, kDummyInt2))
      .WillOnce(Return(kDummyInt3));
  EXPECT_EQ(kDummyInt3, tcflush(kDummyInt, kDummyInt2));
}

TEST_F(KernelWrapTest, tcgetattr) {
  struct termios term;
  EXPECT_CALL(mock, tcgetattr(kDummyInt, &term)).WillOnce(Return(kDummyInt2));
  EXPECT_EQ(kDummyInt2, tcgetattr(kDummyInt, &term));
}

TEST_F(KernelWrapTest, tcsetattr) {
  struct termios term;
  EXPECT_CALL(mock, tcsetattr(kDummyInt, kDummyInt2, &term))
      .WillOnce(Return(kDummyInt3));
  EXPECT_EQ(kDummyInt3, tcsetattr(kDummyInt, kDummyInt2, &term));
}

TEST_F(KernelWrapTest, umount) {
  EXPECT_CALL(mock, umount(kDummyConstChar)).WillOnce(Return(kDummyInt));
  EXPECT_EQ(kDummyInt, umount(kDummyConstChar));
}

TEST_F(KernelWrapTest, truncate) {
  EXPECT_CALL(mock, truncate(kDummyConstChar, kDummyInt3)).WillOnce(Return(-1));
  EXPECT_EQ(-1, truncate(kDummyConstChar, kDummyInt3));

  EXPECT_CALL(mock, truncate(kDummyConstChar, kDummyInt3)).WillOnce(Return(0));
  EXPECT_EQ(0, truncate(kDummyConstChar, kDummyInt3));
}

TEST_F(KernelWrapTest, lstat) {
  struct stat buf;
  EXPECT_CALL(mock, lstat(kDummyConstChar, &buf)).WillOnce(Return(-1));
  EXPECT_EQ(-1, lstat(kDummyConstChar, &buf));

  EXPECT_CALL(mock, lstat(kDummyConstChar, &buf)).WillOnce(Return(0));
  EXPECT_EQ(0, lstat(kDummyConstChar, &buf));
}

TEST_F(KernelWrapTest, unlink) {
  EXPECT_CALL(mock, unlink(kDummyConstChar)).WillOnce(Return(kDummyInt));
  EXPECT_EQ(kDummyInt, unlink(kDummyConstChar));
}

TEST_F(KernelWrapTest, utime) {
  const struct utimbuf* times = NULL;
  EXPECT_CALL(mock, utime(kDummyConstChar, times)).WillOnce(Return(kDummyInt));
  EXPECT_EQ(kDummyInt, utime(kDummyConstChar, times));
}

TEST_F(KernelWrapTest, utimes) {
  struct timeval* times = NULL;
  EXPECT_CALL(mock, utimes(kDummyConstChar, times)).WillOnce(Return(-1));
  EXPECT_EQ(-1, utimes(kDummyConstChar, times));
}

TEST_F(KernelWrapTest, write) {
  EXPECT_CALL(mock, write(kDummyInt, kDummyVoidPtr, kDummyInt2))
      .WillOnce(Return(kDummyInt3));
  EXPECT_EQ(kDummyInt3, write(kDummyInt, kDummyVoidPtr, kDummyInt2));
}

#ifdef PROVIDES_SOCKET_API
TEST_F(KernelWrapTest, poll) {
  struct pollfd fds;
  EXPECT_CALL(mock, poll(&fds, kDummyInt, kDummyInt2))
      .WillOnce(Return(kDummyInt3));
  EXPECT_EQ(kDummyInt3, poll(&fds, kDummyInt, kDummyInt2));
}

TEST_F(KernelWrapTest, select) {
  fd_set readfds;
  fd_set writefds;
  fd_set exceptfds;
  EXPECT_CALL(mock, select(kDummyInt, &readfds, &writefds, &exceptfds, NULL))
      .WillOnce(Return(kDummyInt2));
  EXPECT_EQ(kDummyInt2,
            select(kDummyInt, &readfds, &writefds, &exceptfds, NULL));
}

// Socket Functions
TEST_F(KernelWrapTest, accept) {
  struct sockaddr addr;
  socklen_t len;
  EXPECT_CALL(mock, accept(kDummyInt, &addr, &len))
      .WillOnce(Return(kDummyInt2));
  EXPECT_EQ(kDummyInt2, accept(kDummyInt, &addr, &len));
}

TEST_F(KernelWrapTest, bind) {
  struct sockaddr addr;
  EXPECT_CALL(mock, bind(kDummyInt, &addr, kDummyInt2))
      .WillOnce(Return(kDummyInt2));
  EXPECT_EQ(kDummyInt2, bind(kDummyInt, &addr, kDummyInt2));
}

TEST_F(KernelWrapTest, connect) {
  struct sockaddr addr;
  EXPECT_CALL(mock, connect(kDummyInt, &addr, kDummyInt2))
      .WillOnce(Return(kDummyInt2));
  EXPECT_EQ(kDummyInt2, connect(kDummyInt, &addr, kDummyInt2));
}

TEST_F(KernelWrapTest, gethostbyname) {
  struct hostent result;
  EXPECT_CALL(mock, gethostbyname(kDummyConstChar)).WillOnce(Return(&result));
  EXPECT_EQ(&result, gethostbyname(kDummyConstChar));
}

TEST_F(KernelWrapTest, getpeername) {
  struct sockaddr addr;
  socklen_t len;
  EXPECT_CALL(mock, getpeername(kDummyInt, &addr, &len))
      .WillOnce(Return(kDummyInt2));
  EXPECT_EQ(kDummyInt2, getpeername(kDummyInt, &addr, &len));
}

TEST_F(KernelWrapTest, getsockname) {
  struct sockaddr addr;
  socklen_t len;
  EXPECT_CALL(mock, getsockname(kDummyInt, &addr, &len))
      .WillOnce(Return(kDummyInt2));
  EXPECT_EQ(kDummyInt2, getsockname(kDummyInt, &addr, &len));
}

TEST_F(KernelWrapTest, getsockopt) {
  int dummy_val;
  void* dummy_void_ptr = &dummy_val;
  socklen_t len;
  EXPECT_CALL(
      mock, getsockopt(kDummyInt, kDummyInt2, kDummyInt3, dummy_void_ptr, &len))
      .WillOnce(Return(kDummyInt4));
  EXPECT_EQ(
      kDummyInt4,
      getsockopt(kDummyInt, kDummyInt2, kDummyInt3, dummy_void_ptr, &len));
}

TEST_F(KernelWrapTest, listen) {
  EXPECT_CALL(mock, listen(kDummyInt, kDummyInt2)).WillOnce(Return(kDummyInt3));
  EXPECT_EQ(kDummyInt3, listen(kDummyInt, kDummyInt2));
}

TEST_F(KernelWrapTest, recv) {
  int dummy_val;
  void* dummy_void_ptr = &dummy_val;
  EXPECT_CALL(mock, recv(kDummyInt, dummy_void_ptr, kDummySizeT, kDummyInt2))
      .WillOnce(Return(kDummyInt3));
  EXPECT_EQ(kDummyInt3,
            recv(kDummyInt, dummy_void_ptr, kDummySizeT, kDummyInt2));
}

TEST_F(KernelWrapTest, recvfrom) {
  int dummy_val;
  void* dummy_void_ptr = &dummy_val;
  struct sockaddr addr;
  socklen_t len;
  EXPECT_CALL(
      mock,
      recvfrom(kDummyInt, dummy_void_ptr, kDummyInt2, kDummyInt3, &addr, &len))
      .WillOnce(Return(kDummyInt4));
  EXPECT_EQ(
      kDummyInt4,
      recvfrom(kDummyInt, dummy_void_ptr, kDummyInt2, kDummyInt3, &addr, &len));
}

TEST_F(KernelWrapTest, recvmsg) {
  struct msghdr msg;
  EXPECT_CALL(mock, recvmsg(kDummyInt, &msg, kDummyInt2))
      .WillOnce(Return(kDummyInt3));
  EXPECT_EQ(kDummyInt3, recvmsg(kDummyInt, &msg, kDummyInt2));
}

TEST_F(KernelWrapTest, send) {
  EXPECT_CALL(mock, send(kDummyInt, kDummyVoidPtr, kDummySizeT, kDummyInt2))
      .WillOnce(Return(kDummyInt3));
  EXPECT_EQ(kDummyInt3,
            send(kDummyInt, kDummyVoidPtr, kDummySizeT, kDummyInt2));
}

TEST_F(KernelWrapTest, sendto) {
  const socklen_t kDummySockLen = 0x50cc5;
  struct sockaddr addr;
  EXPECT_CALL(mock,
              sendto(kDummyInt,
                     kDummyVoidPtr,
                     kDummyInt2,
                     kDummyInt3,
                     &addr,
                     kDummySockLen)).WillOnce(Return(kDummyInt4));
  EXPECT_EQ(kDummyInt4,
            sendto(kDummyInt,
                   kDummyVoidPtr,
                   kDummyInt2,
                   kDummyInt3,
                   &addr,
                   kDummySockLen));
}

TEST_F(KernelWrapTest, sendmsg) {
  struct msghdr msg;
  EXPECT_CALL(mock, sendmsg(kDummyInt, &msg, kDummyInt2))
      .WillOnce(Return(kDummyInt3));
  EXPECT_EQ(kDummyInt3, sendmsg(kDummyInt, &msg, kDummyInt2));
}

TEST_F(KernelWrapTest, setsockopt) {
  const socklen_t kDummySockLen = 0x50cc5;
  EXPECT_CALL(
      mock,
      setsockopt(
          kDummyInt, kDummyInt2, kDummyInt3, kDummyVoidPtr, kDummySockLen))
      .WillOnce(Return(kDummyInt4));
  EXPECT_EQ(
      kDummyInt4,
      setsockopt(
          kDummyInt, kDummyInt2, kDummyInt3, kDummyVoidPtr, kDummySockLen));
}

TEST_F(KernelWrapTest, shutdown) {
  EXPECT_CALL(mock, shutdown(kDummyInt, kDummyInt2))
      .WillOnce(Return(kDummyInt3));
  EXPECT_EQ(kDummyInt3, shutdown(kDummyInt, kDummyInt2));
}

TEST_F(KernelWrapTest, socket) {
  EXPECT_CALL(mock, socket(kDummyInt, kDummyInt2, kDummyInt3))
      .WillOnce(Return(kDummyInt4));
  EXPECT_EQ(kDummyInt4, socket(kDummyInt, kDummyInt2, kDummyInt3));
}

TEST_F(KernelWrapTest, socketpair) {
  int dummy_val;
  EXPECT_CALL(mock, socketpair(kDummyInt, kDummyInt2, kDummyInt3, &dummy_val))
      .WillOnce(Return(kDummyInt4));
  EXPECT_EQ(kDummyInt4,
            socketpair(kDummyInt, kDummyInt2, kDummyInt3, &dummy_val));
}

#endif  // PROVIDES_SOCKET_API
