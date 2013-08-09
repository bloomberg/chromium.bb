// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "kernel_proxy_mock.h"
#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_wrap.h"
#include "nacl_io/ossocket.h"
#include "nacl_io/ostermios.h"

using namespace nacl_io;

using ::testing::_;
using ::testing::Return;
using ::testing::StrEq;

namespace {

static const int DUMMY_FD = 5678;

#define COMPARE_FIELD(f) \
  if (arg->f != statbuf->f) { \
    *result_listener << "mismatch of field \""#f"\". " \
      "expected: " << statbuf->f << \
      " actual: " << arg->f; \
    return false; \
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
  return 0;
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

const uid_t kDummyUid = 1001;
const gid_t kDummyGid = 1002;

class KernelWrapTest : public ::testing::Test {
 public:
  KernelWrapTest() {
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

  ~KernelWrapTest() {
    ki_uninit();
  }

  KernelProxyMock mock;
};

}  // namespace


TEST_F(KernelWrapTest, access) {
  EXPECT_CALL(mock, access(StrEq("access"), 12)).Times(1);
  access("access", 12);
}

TEST_F(KernelWrapTest, chdir) {
  EXPECT_CALL(mock, chdir(StrEq("chdir"))).Times(1);
  chdir("chdir");
}

TEST_F(KernelWrapTest, chmod) {
  EXPECT_CALL(mock, chmod(StrEq("chmod"), 23)).Times(1);
  chmod("chmod", 23);
}

TEST_F(KernelWrapTest, chown) {
  uid_t uid = kDummyUid;
  gid_t gid = kDummyGid;
  EXPECT_CALL(mock, chown(StrEq("chown"), uid, gid)).Times(1);
  chown("chown", uid, gid);
}

TEST_F(KernelWrapTest, close) {
  EXPECT_CALL(mock, close(34)).Times(1);
  close(34);
}

TEST_F(KernelWrapTest, dup) {
  EXPECT_CALL(mock, dup(DUMMY_FD)).Times(1);
  dup(DUMMY_FD);
}

TEST_F(KernelWrapTest, dup2) {
  EXPECT_CALL(mock, dup2(DUMMY_FD, 234)).Times(1);
  dup2(DUMMY_FD, 234);
}

TEST_F(KernelWrapTest, fchown) {
  uid_t uid = kDummyUid;
  gid_t gid = kDummyGid;
  EXPECT_CALL(mock, fchown(DUMMY_FD, uid, gid)).Times(1);
  fchown(DUMMY_FD, uid, gid);
}

TEST_F(KernelWrapTest, fstat) {
  struct stat in_statbuf;
  MakeDummyStatbuf(&in_statbuf);
  EXPECT_CALL(mock, fstat(DUMMY_FD, _))
      .Times(1)
      .WillOnce(SetStat(&in_statbuf));
  struct stat out_statbuf;
  fstat(DUMMY_FD, &out_statbuf);
  EXPECT_THAT(&in_statbuf, IsEqualToStatbuf(&out_statbuf));
}

TEST_F(KernelWrapTest, ftruncate) {
  EXPECT_CALL(mock, ftruncate(456, 0)).Times(1);
  ftruncate(456, 0);
}

TEST_F(KernelWrapTest, fsync) {
  EXPECT_CALL(mock, fsync(345)).Times(1);
  fsync(345);
}

TEST_F(KernelWrapTest, getcwd) {
  EXPECT_CALL(mock, getcwd(StrEq("getcwd"), 1)).Times(1);
  char buffer[] = "getcwd";
  getcwd(buffer, 1);
}

TEST_F(KernelWrapTest, getdents) {
  EXPECT_CALL(mock, getdents(456, NULL, 567)).Times(1);
  getdents(456, NULL, 567);
}

// gcc gives error: getwd is deprecated.
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
TEST_F(KernelWrapTest, getwd) {
  EXPECT_CALL(mock, getwd(StrEq("getwd"))).Times(1);
  char buffer[] = "getwd";
  getwd(buffer);
}
#if defined(__GNUC__)
#pragma GCC diagnostic warning "-Wdeprecated-declarations"
#endif

TEST_F(KernelWrapTest, ioctl) {
  char buffer[] = "ioctl";
  EXPECT_CALL(mock, ioctl(012, 345, StrEq("ioctl"))).Times(1);
  ioctl(012, 345, buffer);
}

TEST_F(KernelWrapTest, isatty) {
  EXPECT_CALL(mock, isatty(678)).Times(1);
  isatty(678);
}

TEST_F(KernelWrapTest, lchown) {
  uid_t uid = kDummyUid;
  gid_t gid = kDummyGid;
  EXPECT_CALL(mock, lchown(StrEq("lchown"), uid, gid)).Times(1);
  lchown("lchown", uid, gid);
}

TEST_F(KernelWrapTest, lseek) {
  EXPECT_CALL(mock, lseek(789, 891, 912)).Times(1);
  lseek(789, 891, 912);
}

TEST_F(KernelWrapTest, mkdir) {
#if defined(WIN32)
  EXPECT_CALL(mock, mkdir(StrEq("mkdir"), 0777)).Times(1);
  mkdir("mkdir");
#else
  EXPECT_CALL(mock, mkdir(StrEq("mkdir"), 1234)).Times(1);
  mkdir("mkdir", 1234);
#endif
}

TEST_F(KernelWrapTest, mount) {
  EXPECT_CALL(mock,
      mount(StrEq("mount1"), StrEq("mount2"), StrEq("mount3"), 2345, NULL))
      .Times(1);
  mount("mount1", "mount2", "mount3", 2345, NULL);
}

TEST_F(KernelWrapTest, open) {
  EXPECT_CALL(mock, open(StrEq("open"), 3456)).Times(1);
  open("open", 3456);
}

TEST_F(KernelWrapTest, read) {
  EXPECT_CALL(mock, read(4567, NULL, 5678)).Times(1);
  read(4567, NULL, 5678);
}

TEST_F(KernelWrapTest, remove) {
  EXPECT_CALL(mock, remove(StrEq("remove"))).Times(1);
  remove("remove");
}

TEST_F(KernelWrapTest, rmdir) {
  EXPECT_CALL(mock, rmdir(StrEq("rmdir"))).Times(1);
  rmdir("rmdir");
}

TEST_F(KernelWrapTest, stat) {
  struct stat in_statbuf;
  MakeDummyStatbuf(&in_statbuf);
  EXPECT_CALL(mock, stat(StrEq("stat"), _))
      .Times(1)
      .WillOnce(SetStat(&in_statbuf));
  struct stat out_statbuf;
  stat("stat", &out_statbuf);
  EXPECT_THAT(&in_statbuf, IsEqualToStatbuf(&out_statbuf));
}

TEST_F(KernelWrapTest, tcgetattr) {
  struct termios term;
  EXPECT_CALL(mock, tcgetattr(DUMMY_FD, &term)).Times(1);
  tcgetattr(DUMMY_FD, &term);
}

TEST_F(KernelWrapTest, tcsetattr) {
  struct termios term;
  EXPECT_CALL(mock, tcsetattr(DUMMY_FD, 0, &term)).Times(1);
  tcsetattr(DUMMY_FD, 0, &term);
}

TEST_F(KernelWrapTest, umount) {
  EXPECT_CALL(mock, umount(StrEq("umount"))).Times(1);
  umount("umount");
}

TEST_F(KernelWrapTest, unlink) {
  EXPECT_CALL(mock, unlink(StrEq("unlink"))).Times(1);
  unlink("unlink");
}

TEST_F(KernelWrapTest, utime) {
  const struct utimbuf* times = NULL;
  EXPECT_CALL(mock, utime(StrEq("utime"), times));
  utime("utime", times);
}

TEST_F(KernelWrapTest, write) {
  EXPECT_CALL(mock, write(6789, NULL, 7891)).Times(1);
  write(6789, NULL, 7891);
}

#ifdef PROVIDES_SOCKET_API
TEST_F(KernelWrapTest, poll) {
  EXPECT_CALL(mock, poll(NULL, 5, -1));
  poll(NULL, 5, -1);
}

TEST_F(KernelWrapTest, select) {
  EXPECT_CALL(mock, select(123, NULL, NULL, NULL, NULL));
  select(123, NULL, NULL, NULL, NULL);
}

// Socket Functions
TEST_F(KernelWrapTest, accept) {
  EXPECT_CALL(mock, accept(DUMMY_FD, NULL, NULL)).Times(1);
  accept(DUMMY_FD, NULL, NULL);
}

TEST_F(KernelWrapTest, bind) {
  EXPECT_CALL(mock, bind(DUMMY_FD, NULL, 456)).Times(1);
  bind(DUMMY_FD, NULL, 456);
}

TEST_F(KernelWrapTest, connect) {
  EXPECT_CALL(mock, connect(DUMMY_FD, NULL, 456)).Times(1);
  connect(DUMMY_FD, NULL, 456);
}

TEST_F(KernelWrapTest, gethostbyname) {
  EXPECT_CALL(mock, gethostbyname(NULL)).Times(1);
  gethostbyname(NULL);
}

TEST_F(KernelWrapTest, getpeername) {
  EXPECT_CALL(mock, getpeername(DUMMY_FD, NULL, NULL)).Times(1);
  getpeername(DUMMY_FD, NULL, NULL);
}

TEST_F(KernelWrapTest, getsockname) {
  EXPECT_CALL(mock, getsockname(DUMMY_FD, NULL, NULL)).Times(1);
  getsockname(DUMMY_FD, NULL, NULL);
}

TEST_F(KernelWrapTest, getsockopt) {
  EXPECT_CALL(mock, getsockopt(DUMMY_FD, 456, 789, NULL, NULL)).Times(1);
  getsockopt(DUMMY_FD, 456, 789, NULL, NULL);
}

TEST_F(KernelWrapTest, listen) {
  EXPECT_CALL(mock, listen(DUMMY_FD, 456)).Times(1);
  listen(DUMMY_FD, 456);
}

TEST_F(KernelWrapTest, recv) {
  EXPECT_CALL(mock, recv(DUMMY_FD, NULL, 456, 789)).Times(1);
  recv(DUMMY_FD, NULL, 456, 789);
}

TEST_F(KernelWrapTest, recvfrom) {
  EXPECT_CALL(mock, recvfrom(DUMMY_FD, NULL, 456, 789, NULL, NULL)).Times(1);
  recvfrom(DUMMY_FD, NULL, 456, 789, NULL, NULL);
}

TEST_F(KernelWrapTest, recvmsg) {
  EXPECT_CALL(mock, recvmsg(DUMMY_FD, NULL, 456)).Times(1);
  recvmsg(DUMMY_FD, NULL, 456);
}

TEST_F(KernelWrapTest, send) {
  EXPECT_CALL(mock, send(DUMMY_FD, NULL, 456, 789)).Times(1);
  send(DUMMY_FD, NULL, 456, 789);
}

TEST_F(KernelWrapTest, sendto) {
  EXPECT_CALL(mock, sendto(DUMMY_FD, NULL, 456, 789, NULL, 314)).Times(1);
  sendto(DUMMY_FD, NULL, 456, 789, NULL, 314);
}

TEST_F(KernelWrapTest, sendmsg) {
  EXPECT_CALL(mock, sendmsg(DUMMY_FD, NULL, 456)).Times(1);
  sendmsg(DUMMY_FD, NULL, 456);
}

TEST_F(KernelWrapTest, setsockopt) {
  EXPECT_CALL(mock, setsockopt(DUMMY_FD, 456, 789, NULL, 314)).Times(1);
  setsockopt(DUMMY_FD, 456, 789, NULL, 314);
}

TEST_F(KernelWrapTest, shutdown) {
  EXPECT_CALL(mock, shutdown(DUMMY_FD, 456)).Times(1);
  shutdown(DUMMY_FD, 456);
}

TEST_F(KernelWrapTest, socket) {
  EXPECT_CALL(mock, socket(DUMMY_FD, 456, 789)).Times(1);
  socket(DUMMY_FD, 456, 789);
}

TEST_F(KernelWrapTest, socketpair) {
  EXPECT_CALL(mock, socketpair(DUMMY_FD, 456, 789, NULL)).Times(1);
  socketpair(DUMMY_FD, 456, 789, NULL);
}

#endif // PROVIDES_SOCKET_API
