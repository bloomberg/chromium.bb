// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>

#include <map>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_proxy.h"
#include "nacl_io/ossocket.h"
#include "nacl_io/ostypes.h"

#ifdef PROVIDES_SOCKET_API

using namespace nacl_io;
using namespace sdk_util;

namespace {
class SocketTest : public ::testing::Test {
 public:
  SocketTest() : kp_(new KernelProxy) {
    ki_init(kp_);
  }

  ~SocketTest() {
    ki_uninit();
    delete kp_;
  }

 protected:
  KernelProxy* kp_;
};

} // namespace

TEST_F(SocketTest, Accept) {
  struct sockaddr addr = {};
  socklen_t len = 0;

  EXPECT_LT(ki_accept(123, NULL, &len), 0);
  EXPECT_EQ(errno, EFAULT);
  EXPECT_LT(ki_accept(123, &addr, NULL), 0);
  EXPECT_EQ(errno, EFAULT);
  EXPECT_LT(ki_accept(123, NULL, NULL), 0);
  EXPECT_EQ(errno, EFAULT);
  EXPECT_LT(ki_accept(-1, &addr, &len), 0);
  EXPECT_EQ(errno, EBADF);
  EXPECT_LT(ki_accept(0, &addr, &len), 0);
  EXPECT_EQ(errno, ENOTSOCK);
}

TEST_F(SocketTest, Bind) {
  const struct sockaddr const_addr = {};
  socklen_t len = 0;

  EXPECT_LT(ki_bind(123, NULL, len), 0);
  EXPECT_EQ(errno, EFAULT);
  EXPECT_LT(ki_bind(-1, &const_addr, len), 0);
  EXPECT_EQ(errno, EBADF);
  EXPECT_LT(ki_bind(0, &const_addr, len), 0);
  EXPECT_EQ(errno, ENOTSOCK);
}

TEST_F(SocketTest, Connect) {
  const struct sockaddr const_addr = {};
  socklen_t len = 0;

  EXPECT_LT(ki_connect(123, NULL, len), 0);
  EXPECT_EQ(errno, EFAULT);
  EXPECT_LT(ki_connect(-1, &const_addr, len), 0);
  EXPECT_EQ(errno, EBADF);
  EXPECT_LT(ki_connect(0, &const_addr, len), 0);
  EXPECT_EQ(errno, ENOTSOCK);
}

TEST_F(SocketTest, Getpeername) {
  struct sockaddr addr = {};
  socklen_t len = 0;

  EXPECT_LT(ki_getpeername(123, NULL, &len), 0);
  EXPECT_EQ(errno, EFAULT);
  EXPECT_LT(ki_getpeername(123, &addr, NULL), 0);
  EXPECT_EQ(errno, EFAULT);
  EXPECT_LT(ki_getpeername(123, NULL, NULL), 0);
  EXPECT_EQ(errno, EFAULT);
  EXPECT_LT(ki_getpeername(-1, &addr, &len), 0);
  EXPECT_EQ(errno, EBADF);
  EXPECT_LT(ki_getpeername(0, &addr, &len), 0);
  EXPECT_EQ(errno, ENOTSOCK);
}

TEST_F(SocketTest, Getsockname) {
  struct sockaddr addr = {};
  socklen_t len = 0;

  EXPECT_LT(ki_getsockname(123, NULL, &len), 0);
  EXPECT_EQ(errno, EFAULT);
  EXPECT_LT(ki_getsockname(123, &addr, NULL), 0);
  EXPECT_EQ(errno, EFAULT);
  EXPECT_LT(ki_getsockname(123, NULL, NULL), 0);
  EXPECT_EQ(errno, EFAULT);
  EXPECT_LT(ki_getsockname(-1, &addr, &len), 0);
  EXPECT_EQ(errno, EBADF);
  EXPECT_LT(ki_getsockname(0, &addr, &len), 0);
  EXPECT_EQ(errno, ENOTSOCK);
}

TEST_F(SocketTest, Getsockopt) {
  socklen_t len = 10;
  char optval[len];

  EXPECT_LT(ki_getsockopt(123, SOL_SOCKET, SO_ACCEPTCONN, optval, NULL), 0);
  EXPECT_EQ(errno, EFAULT);
  EXPECT_LT(ki_getsockopt(123, SOL_SOCKET, SO_ACCEPTCONN, NULL, &len), 0);
  EXPECT_EQ(errno, EFAULT);
  EXPECT_LT(ki_getsockopt(123, SOL_SOCKET, SO_ACCEPTCONN, NULL, NULL), 0);
  EXPECT_EQ(errno, EFAULT);
  EXPECT_LT(ki_getsockopt(-1, SOL_SOCKET, SO_ACCEPTCONN, optval, &len), 0);
  EXPECT_EQ(errno, EBADF);
  EXPECT_LT(ki_getsockopt(0, SOL_SOCKET, SO_ACCEPTCONN, optval, &len), 0);
  EXPECT_EQ(errno, ENOTSOCK);
}

TEST_F(SocketTest, Listen) {
  EXPECT_LT(ki_listen(-1, 123), 0);
  EXPECT_EQ(errno, EBADF);
  EXPECT_LT(ki_listen(0, 123), 0);
  EXPECT_EQ(errno, ENOTSOCK);
}

TEST_F(SocketTest, Recv) {
  size_t len = 10;
  char buf[len];

  EXPECT_LT(ki_recv(123, NULL, len, 0), 0);
  EXPECT_EQ(errno, EFAULT);
  EXPECT_LT(ki_recv(-1, buf, len, 0), 0);
  EXPECT_EQ(errno, EBADF);
  EXPECT_LT(ki_recv(0, buf, len, 0), 0);
  EXPECT_EQ(errno, ENOTSOCK);
}

TEST_F(SocketTest, Recvfrom) {
  size_t len = 10;
  char buf[len];
  struct sockaddr addr = {};
  socklen_t addrlen = 4;

  EXPECT_LT(ki_recvfrom(123, NULL, len, 0, &addr, &addrlen), 0);
  EXPECT_EQ(errno, EFAULT);
  EXPECT_LT(ki_recvfrom(123, buf, len, 0, &addr, NULL), 0);
  EXPECT_EQ(errno, EFAULT);
  EXPECT_LT(ki_recvfrom(-1, buf, len, 0, &addr, &addrlen), 0);
  EXPECT_EQ(errno, EBADF);
  EXPECT_LT(ki_recvfrom(0, buf, len, 0, &addr, &addrlen), 0);
  EXPECT_EQ(errno, ENOTSOCK);
}

TEST_F(SocketTest, Recvmsg) {
  struct msghdr msg = {};

  EXPECT_LT(ki_recvmsg(123, NULL, 0), 0);
  EXPECT_EQ(errno, EFAULT);
  EXPECT_LT(ki_recvmsg(-1, &msg, 0), 0);
  EXPECT_EQ(errno, EBADF);
  EXPECT_LT(ki_recvmsg(0, &msg, 0), 0);
  EXPECT_EQ(errno, ENOTSOCK);
}

TEST_F(SocketTest, Send) {
  size_t len = 10;
  char buf[len];

  EXPECT_LT(ki_send(123, NULL, len, 0), 0);
  EXPECT_EQ(errno, EFAULT);
  EXPECT_LT(ki_send(-1, buf, len, 0), 0);
  EXPECT_EQ(errno, EBADF);
  EXPECT_LT(ki_send(0, buf, len, 0), 0);
  EXPECT_EQ(errno, ENOTSOCK);
}

TEST_F(SocketTest, Sendto) {
  size_t len = 10;
  char buf[len];
  struct sockaddr addr = {};
  socklen_t addrlen = 4;

  EXPECT_LT(ki_sendto(123, NULL, len, 0, &addr, addrlen), 0);
  EXPECT_EQ(errno, EFAULT);
  EXPECT_LT(ki_sendto(-1, buf, len, 0, &addr, addrlen), 0);
  EXPECT_EQ(errno, EBADF);
  EXPECT_LT(ki_sendto(0, buf, len, 0, &addr, addrlen), 0);
  EXPECT_EQ(errno, ENOTSOCK);
}

TEST_F(SocketTest, Sendmsg) {
  struct msghdr msg = {};

  EXPECT_LT(ki_sendmsg(123, NULL, 0), 0);
  EXPECT_EQ(errno, EFAULT);
  EXPECT_LT(ki_sendmsg(-1, &msg, 0), 0);
  EXPECT_EQ(errno, EBADF);
  EXPECT_LT(ki_sendmsg(0, &msg, 0), 0);
  EXPECT_EQ(errno, ENOTSOCK);
}

TEST_F(SocketTest, Setsockopt) {
  socklen_t len = 10;
  char optval[len];

  EXPECT_LT(ki_setsockopt(123, SOL_SOCKET, SO_ACCEPTCONN, NULL, len), 0);
  EXPECT_EQ(errno, EFAULT);
  EXPECT_LT(ki_setsockopt(-1, SOL_SOCKET, SO_ACCEPTCONN, optval, len), 0);
  EXPECT_EQ(errno, EBADF);
  EXPECT_LT(ki_setsockopt(0, SOL_SOCKET, SO_ACCEPTCONN, optval, len), 0);
  EXPECT_EQ(errno, ENOTSOCK);
}

TEST_F(SocketTest, Shutdown) {
  EXPECT_LT(ki_shutdown(-1, SHUT_RDWR), 0);
  EXPECT_EQ(errno, EBADF);
  EXPECT_LT(ki_shutdown(0, SHUT_RDWR), 0);
  EXPECT_EQ(errno, ENOTSOCK);
}

TEST_F(SocketTest, Socket) {
  EXPECT_LT(ki_socket(AF_UNIX, SOCK_STREAM, 0), 0);
  EXPECT_EQ(errno, EAFNOSUPPORT);
  EXPECT_LT(ki_socket(AF_INET, SOCK_RAW, 0), 0);
  EXPECT_EQ(errno, EPROTONOSUPPORT);

  // These four af/protocol combinations should be supported.
  EXPECT_LT(ki_socket(AF_INET, SOCK_STREAM, 0), 0);
  EXPECT_EQ(errno, EACCES);
  EXPECT_LT(ki_socket(AF_INET, SOCK_DGRAM, 0), 0);
  EXPECT_EQ(errno, EACCES);
  EXPECT_LT(ki_socket(AF_INET6, SOCK_STREAM, 0), 0);
  EXPECT_EQ(errno, EACCES);
  EXPECT_LT(ki_socket(AF_INET6, SOCK_DGRAM, 0), 0);
  EXPECT_EQ(errno, EACCES);
}

TEST_F(SocketTest, Socketpair) {
  int sv[2];
  EXPECT_LT(ki_socketpair(AF_INET, SOCK_STREAM, 0, NULL), 0);
  EXPECT_EQ(errno, EFAULT);
  EXPECT_LT(ki_socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);
  EXPECT_EQ(errno, EAFNOSUPPORT);
  EXPECT_LT(ki_socketpair(AF_INET, SOCK_STREAM, 0, sv), 0);
  EXPECT_EQ(errno, EPROTONOSUPPORT);
  EXPECT_LT(ki_socketpair(AF_INET6, SOCK_STREAM, 0, sv), 0);
  EXPECT_EQ(errno, EPROTONOSUPPORT);
}

#endif // PROVIDES_SOCKETPAIR_API
