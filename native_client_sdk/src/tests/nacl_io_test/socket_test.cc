// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <map>

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
  SocketTest() {}

  void SetUp() {
    ASSERT_EQ(0, ki_push_state_for_testing());
    ASSERT_EQ(0, ki_init(&kp_));
  }

  void TearDown() {
    ki_uninit();
  }

 protected:
  KernelProxy kp_;
};

}  // namespace

TEST_F(SocketTest, Accept) {
  struct sockaddr addr = {};
  socklen_t len = 0;

  // accept() should allow NULL args for addr and len
  // https://code.google.com/p/chromium/issues/detail?id=442164
  // EXPECT_LT(ki_accept(123, NULL, &len), 0);
  // EXPECT_EQ(errno, EFAULT);
  // EXPECT_LT(ki_accept(123, &addr, NULL), 0);
  // EXPECT_EQ(errno, EFAULT);
  // EXPECT_LT(ki_accept(123, NULL, NULL), 0);
  // EXPECT_EQ(errno, EFAULT);
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

  // Passing a bad address as optval should generate EFAULT
  EXPECT_EQ(-1, ki_setsockopt(123, SOL_SOCKET, SO_ACCEPTCONN, NULL, len));
  EXPECT_EQ(errno, EFAULT);

  // Passing a bad socket descriptor should generate EBADF
  EXPECT_EQ(-1, ki_setsockopt(-1, SOL_SOCKET, SO_ACCEPTCONN, optval, len));
  EXPECT_EQ(errno, EBADF);

  // Passing an FD that is valid but not a socket should generate ENOTSOCK
  EXPECT_EQ(-1, ki_setsockopt(0, SOL_SOCKET, SO_ACCEPTCONN, optval, len));
  EXPECT_EQ(errno, ENOTSOCK);
}

TEST_F(SocketTest, Shutdown) {
  EXPECT_LT(ki_shutdown(-1, SHUT_RDWR), 0);
  EXPECT_EQ(errno, EBADF);
  EXPECT_LT(ki_shutdown(0, SHUT_RDWR), 0);
  EXPECT_EQ(errno, ENOTSOCK);
}

TEST_F(SocketTest, SocketInetRawUnsupported) {
  EXPECT_LT(ki_socket(AF_INET, SOCK_RAW, 0), 0);
  EXPECT_EQ(errno, EPROTONOSUPPORT);
}

TEST_F(SocketTest, SocketpairUnsupported) {
  int sv[2];
  EXPECT_LT(ki_socketpair(AF_INET, SOCK_STREAM, 0, NULL), 0);
  EXPECT_EQ(errno, EFAULT);
  EXPECT_LT(ki_socketpair(AF_INET, SOCK_STREAM, 0, sv), 0);
  EXPECT_EQ(errno, EOPNOTSUPP);
  EXPECT_LT(ki_socketpair(AF_INET6, SOCK_STREAM, 0, sv), 0);
  EXPECT_EQ(errno, EOPNOTSUPP);
  EXPECT_LT(ki_socketpair(AF_UNIX, SOCK_RAW, 0, sv), 0);
  EXPECT_EQ(errno, EPROTOTYPE);
  EXPECT_LT(ki_socketpair(AF_MAX, SOCK_STREAM, 0, sv), 0);
  EXPECT_EQ(errno, EAFNOSUPPORT);
}

class UnixSocketTest : public ::testing::Test {
 public:
  UnixSocketTest() { sv_[0] = sv_[1] = -1; }

  void SetUp() {
    ASSERT_EQ(0, ki_push_state_for_testing());
    ASSERT_EQ(0, ki_init(&kp_));
  }

  void TearDown() {
    if (sv_[0] != -1)
      EXPECT_EQ(0, ki_close(sv_[0]));
    if (sv_[1] != -1)
      EXPECT_EQ(0, ki_close(sv_[1]));
    ki_uninit();
  }

 protected:
  KernelProxy kp_;

  int sv_[2];
};

TEST_F(UnixSocketTest, Socket) {
  EXPECT_EQ(-1, ki_socket(AF_UNIX, SOCK_STREAM, 0));
  EXPECT_EQ(EAFNOSUPPORT, errno);
}

TEST_F(UnixSocketTest, Socketpair) {
  errno = 0;
  EXPECT_EQ(0, ki_socketpair(AF_UNIX, SOCK_STREAM, 0, sv_));
  EXPECT_EQ(0, errno);
  EXPECT_LE(0, sv_[0]);
  EXPECT_LE(0, sv_[1]);

  errno = 0;
  EXPECT_EQ(0, ki_socketpair(AF_UNIX, SOCK_DGRAM, 0, sv_));
  EXPECT_EQ(0, errno);
  EXPECT_LE(0, sv_[0]);
  EXPECT_LE(0, sv_[1]);
}

TEST_F(UnixSocketTest, SendRecvStream) {
  char outbuf[256];
  char inbuf[512];

  memset(outbuf, 0xA5, sizeof(outbuf));
  memset(inbuf, 0x3C, sizeof(inbuf));

  EXPECT_EQ(0, ki_socketpair(AF_UNIX, SOCK_STREAM, 0, sv_));

  int len1 = ki_send(sv_[0], outbuf, sizeof(outbuf), /* flags */ 0);
  EXPECT_EQ(sizeof(outbuf), len1);

  // The buffers should be different.
  EXPECT_NE(0, memcmp(outbuf, inbuf, sizeof(outbuf)));

  int len2 = ki_recv(sv_[1], inbuf, sizeof(inbuf), /* flags */ 0);
  EXPECT_EQ(sizeof(outbuf), len2);

  EXPECT_EQ(0, memcmp(outbuf, inbuf, sizeof(outbuf)));

  // A reader should block after trying to read at this point.
  EXPECT_EQ(-1, ki_recv(sv_[1], inbuf, sizeof(inbuf), MSG_DONTWAIT));
  EXPECT_EQ(EAGAIN, errno);

  // Send data back in the opposite direction.
  memset(inbuf, 0x3C, sizeof(inbuf));
  EXPECT_NE(0, memcmp(outbuf, inbuf, sizeof(outbuf)));
  len1 = ki_send(sv_[1], outbuf, sizeof(outbuf), /* flags */ 0);
  EXPECT_EQ(sizeof(outbuf), len1);

  EXPECT_NE(0, memcmp(outbuf, inbuf, sizeof(outbuf)));

  len2 = ki_recv(sv_[0], inbuf, sizeof(inbuf), /* flags */ 0);
  EXPECT_EQ(sizeof(outbuf), len2);

  EXPECT_EQ(0, memcmp(outbuf, inbuf, sizeof(outbuf)));
  EXPECT_EQ(-1, ki_recv(sv_[0], inbuf, sizeof(inbuf), MSG_DONTWAIT));
  EXPECT_EQ(EAGAIN, errno);
}

TEST_F(UnixSocketTest, RecvNonBlockingStream) {
  char buf[128];

  EXPECT_EQ(0, ki_socketpair(AF_UNIX, SOCK_STREAM, 0, sv_));

  EXPECT_EQ(-1, ki_recv(sv_[0], buf, sizeof(buf), MSG_DONTWAIT));
  EXPECT_EQ(EAGAIN, errno);

  struct pollfd pollfd = {sv_[0], POLLIN | POLLOUT, 0};
  EXPECT_EQ(1, ki_poll(&pollfd, 1, 0));
  EXPECT_EQ(POLLOUT, pollfd.revents & POLLOUT);
  EXPECT_NE(POLLIN, pollfd.revents & POLLIN);
}

TEST_F(UnixSocketTest, SendRecvDgram) {
  char outbuf1[256];
  char outbuf2[128];
  char inbuf[512];

  memset(outbuf1, 0xA4, sizeof(outbuf1));
  memset(outbuf2, 0xA5, sizeof(outbuf2));
  memset(inbuf, 0x3C, sizeof(inbuf));

  EXPECT_EQ(0, ki_socketpair(AF_UNIX, SOCK_DGRAM, 0, sv_));

  int len1 = ki_send(sv_[0], outbuf1, sizeof(outbuf1), /* flags */ 0);
  EXPECT_EQ(sizeof(outbuf1), len1);

  // The buffers should be different.
  EXPECT_NE(0, memcmp(outbuf1, inbuf, sizeof(outbuf1)));

  int len2 = ki_send(sv_[0], outbuf2, sizeof(outbuf2), /* flags */ 0);
  EXPECT_EQ(sizeof(outbuf2), len2);

  // Make sure the datagram boundaries are respected.
  len1 = ki_recv(sv_[1], inbuf, sizeof(inbuf), /* flags */ 0);
  EXPECT_EQ(sizeof(outbuf1), len1);
  EXPECT_EQ(0, memcmp(outbuf1, inbuf, sizeof(outbuf1)));

  len2 = ki_recv(sv_[1], inbuf, sizeof(inbuf), /* flags */ 0);
  EXPECT_EQ(sizeof(outbuf2), len2);
  EXPECT_EQ(0, memcmp(outbuf2, inbuf, sizeof(outbuf2)));

  // A reader should block after trying to read at this point.
  EXPECT_EQ(-1, ki_recv(sv_[1], inbuf, sizeof(inbuf), MSG_DONTWAIT));
  EXPECT_EQ(EAGAIN, errno);

  // Send a datagram larger than the recv buffer, and check for overflow.
  memset(inbuf, 0x3C, sizeof(inbuf));
  EXPECT_NE(0, memcmp(outbuf1, inbuf, sizeof(outbuf1)));
  len1 = ki_send(sv_[1], outbuf1, sizeof(outbuf1), /* flags */ 0);
  EXPECT_EQ(sizeof(outbuf1), len1);

  len2 = ki_recv(sv_[0], inbuf, 16, /* flags */ 0);
  EXPECT_EQ(16, len2);
  EXPECT_EQ(0, memcmp(outbuf1, inbuf, 16));
  EXPECT_EQ(0x3C, inbuf[16]);

  // Verify that the remainder of the packet was discarded, and there
  // is nothing left to receive.
  EXPECT_EQ(-1, ki_recv(sv_[0], inbuf, sizeof(inbuf), MSG_DONTWAIT));
  EXPECT_EQ(EAGAIN, errno);
}

TEST_F(UnixSocketTest, RecvNonBlockingDgram) {
  char buf[128];

  EXPECT_EQ(0, ki_socketpair(AF_UNIX, SOCK_DGRAM, 0, sv_));

  EXPECT_EQ(-1, ki_recv(sv_[0], buf, sizeof(buf), MSG_DONTWAIT));
  EXPECT_EQ(EAGAIN, errno);

  struct pollfd pollfd = {sv_[0], POLLIN | POLLOUT, 0};
  EXPECT_EQ(1, ki_poll(&pollfd, 1, 0));
  EXPECT_EQ(POLLOUT, pollfd.revents & POLLOUT);
  EXPECT_NE(POLLIN, pollfd.revents & POLLIN);
}

TEST(SocketUtilityFunctions, Htonl) {
  uint32_t host_long = 0x44332211;
  uint32_t network_long = htonl(host_long);
  uint8_t network_bytes[4];
  memcpy(network_bytes, &network_long, 4);
  EXPECT_EQ(network_bytes[0], 0x44);
  EXPECT_EQ(network_bytes[1], 0x33);
  EXPECT_EQ(network_bytes[2], 0x22);
  EXPECT_EQ(network_bytes[3], 0x11);
}

TEST(SocketUtilityFunctions, Htons) {
  uint16_t host_short = 0x2211;
  uint16_t network_short = htons(host_short);
  uint8_t network_bytes[2];
  memcpy(network_bytes, &network_short, 2);
  EXPECT_EQ(network_bytes[0], 0x22);
  EXPECT_EQ(network_bytes[1], 0x11);
}

static struct in_addr generate_ipv4_addr(uint8_t* tuple) {
  unsigned char addr[4];
  addr[0] = static_cast<unsigned char>(tuple[0]);
  addr[1] = static_cast<unsigned char>(tuple[1]);
  addr[2] = static_cast<unsigned char>(tuple[2]);
  addr[3] = static_cast<unsigned char>(tuple[3]);
  struct in_addr real_addr;
  memcpy(&real_addr, addr, 4);
  return real_addr;
}

static struct in6_addr generate_ipv6_addr(uint16_t* tuple) {
  unsigned char addr[16];
  for (int i = 0; i < 8; i++) {
    addr[2*i] = (tuple[i] >> 8) & 0xFF;
    addr[2*i+1] = tuple[i] & 0xFF;
  }
  struct in6_addr real_addr;
  memcpy(&real_addr, addr, 16);
  return real_addr;
}

TEST(SocketUtilityFunctions, Inet_addr) {
   // Fails for if string contains non-integers.
   ASSERT_EQ(INADDR_NONE, inet_addr("foobar"));

   // Fails if there are too many quads
   ASSERT_EQ(INADDR_NONE, inet_addr("0.0.0.0.0"));

   // Fails if a single element is > 255
   ASSERT_EQ(INADDR_NONE, inet_addr("999.0.0.0"));

   // Fails if a single element is negative.
   ASSERT_EQ(INADDR_NONE, inet_addr("-55.0.0.0"));

   // In tripple, notation third integer cannot be larger
   // and 16bit unsigned int.
   ASSERT_EQ(INADDR_NONE, inet_addr("1.2.66000"));

   // Success cases.
   // Normal dotted-quad address.
   uint32_t expected_addr = ntohl(0x07060504);
   ASSERT_EQ(expected_addr, inet_addr("7.6.5.4"));
   expected_addr = ntohl(0xffffffff);
   ASSERT_EQ(expected_addr, inet_addr("255.255.255.255"));

   // Tripple case
   expected_addr = ntohl(1 << 24 | 2 << 16 | 3);
   ASSERT_EQ(expected_addr, inet_addr("1.2.3"));
   expected_addr = ntohl(1 << 24 | 2 << 16 | 300);
   ASSERT_EQ(expected_addr, inet_addr("1.2.300"));

   // Double case
   expected_addr = ntohl(1 << 24 | 20000);
   ASSERT_EQ(expected_addr, inet_addr("1.20000"));
   expected_addr = ntohl(1 << 24 | 2);
   ASSERT_EQ(expected_addr, inet_addr("1.2"));

   // Single case
   expected_addr = ntohl(255);
   ASSERT_EQ(expected_addr, inet_addr("255"));
   expected_addr = ntohl(4000000000U);
   ASSERT_EQ(expected_addr, inet_addr("4000000000"));
}

TEST(SocketUtilityFunctions, Inet_aton) {
   struct in_addr addr;

   // Failure cases
   ASSERT_EQ(0, inet_aton("foobar", &addr));
   ASSERT_EQ(0, inet_aton("0.0.0.0.0", &addr));
   ASSERT_EQ(0, inet_aton("999.0.0.0", &addr));

   // Success cases
   uint32_t expected_addr = htonl(0xff020304);
   ASSERT_NE(0, inet_aton("255.2.3.4", &addr));
   ASSERT_EQ(expected_addr, addr.s_addr);

   expected_addr = htonl(0x01000002);
   ASSERT_NE(0, inet_aton("1.2", &addr));
   ASSERT_EQ(expected_addr, addr.s_addr);

   expected_addr = htonl(0x01020003);
   ASSERT_NE(0, inet_aton("1.2.3", &addr));
   ASSERT_EQ(expected_addr, addr.s_addr);

   expected_addr = htonl(0x0000100);
   ASSERT_NE(0, inet_aton("256", &addr));
   ASSERT_EQ(expected_addr, addr.s_addr);
}

TEST(SocketUtilityFunctions, Inet_ntoa) {
  struct {
    unsigned char addr_tuple[4];
    const char* output;
  } tests[] = {
    { { 0,   0,   0,   0   }, "0.0.0.0" },
    { { 127, 0,   0,   1   }, "127.0.0.1" },
    { { 255, 255, 255, 255 }, "255.255.255.255" },
  };

  for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
    char* stringified_addr = inet_ntoa(generate_ipv4_addr(tests[i].addr_tuple));
    ASSERT_TRUE(NULL != stringified_addr);
    EXPECT_STREQ(tests[i].output, stringified_addr);
  }
}

TEST(SocketUtilityFunctions, Inet_ntop_ipv4) {
  struct {
    unsigned char addr_tuple[4];
    const char* output;
  } tests[] = {
    { { 0,   0,   0,   0   }, "0.0.0.0" },
    { { 127, 0,   0,   1   }, "127.0.0.1" },
    { { 255, 255, 255, 255 }, "255.255.255.255" },
  };

  for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
    char stringified_addr[INET_ADDRSTRLEN];
    struct in_addr real_addr = generate_ipv4_addr(tests[i].addr_tuple);
    EXPECT_TRUE(NULL != inet_ntop(AF_INET, &real_addr,
                                  stringified_addr, INET_ADDRSTRLEN));
    EXPECT_STREQ(tests[i].output, stringified_addr);
  }
}

TEST(SocketUtilityFunctions, Inet_ntop_ipv6) {
  struct {
    unsigned short addr_tuple[8];
    const char* output;
  } tests[] = {
    { { 0, 0, 0, 0, 0, 0, 0, 0 }, "::" },
    { { 1, 2, 3, 0, 0, 0, 0, 0 }, "1:2:3::" },
    { { 0, 0, 0, 0, 0, 1, 2, 3 }, "::1:2:3" },
    { { 0x1234, 0xa, 0x12, 0x0000, 0x5678, 0x9abc, 0xdef, 0xffff },
      "1234:a:12:0:5678:9abc:def:ffff" },
    { { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff },
      "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff" },
    { { 0, 0, 0, 0, 0, 0xffff, 0x101, 0x101 }, "::ffff:1.1.1.1" },
    { { 0, 0, 0, 0, 0, 0, 0x101, 0x101 }, "::1.1.1.1" },
  };

  for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
    char stringified_addr[INET6_ADDRSTRLEN];
    struct in6_addr real_addr = generate_ipv6_addr(tests[i].addr_tuple);
    EXPECT_TRUE(NULL != inet_ntop(AF_INET6, &real_addr,
                                  stringified_addr, INET6_ADDRSTRLEN));
    EXPECT_STREQ(tests[i].output, stringified_addr);
  }
}

TEST(SocketUtilityFunctions, Inet_ntop_failure) {
  char addr_name[INET6_ADDRSTRLEN];
  uint16_t addr6_tuple[8] = { 0xffff, 0xffff, 0xffff, 0xffff,
                              0xffff, 0xffff, 0xffff, 0xffff };
  uint8_t addr_tuple[4] = { 255, 255, 255, 255 };
  struct in6_addr ipv6_addr = generate_ipv6_addr(addr6_tuple);
  struct in_addr ipv4_addr = generate_ipv4_addr(addr_tuple);

  EXPECT_EQ(NULL, inet_ntop(AF_UNIX, &ipv6_addr,
                            addr_name, INET6_ADDRSTRLEN));
  EXPECT_EQ(EAFNOSUPPORT, errno);

  EXPECT_EQ(NULL, inet_ntop(AF_INET, &ipv4_addr,
                            addr_name, INET_ADDRSTRLEN - 1));
  EXPECT_EQ(ENOSPC, errno);

  EXPECT_EQ(NULL, inet_ntop(AF_INET6, &ipv6_addr,
                            addr_name, INET6_ADDRSTRLEN / 2));
  EXPECT_EQ(ENOSPC, errno);
}

TEST(SocketUtilityFunctions, Inet_pton) {
  struct {
    int family;
    const char* input;
    const char* output; // NULL means output should match input
  } tests[] = {
    { AF_INET, "127.127.12.0", NULL },
    { AF_INET, "0.0.0.0", NULL },

    { AF_INET6, "0:0:0:0:0:0:0:0", "::" },
    { AF_INET6, "1234:5678:9abc:def0:1234:5678:9abc:def0", NULL },
    { AF_INET6, "1:2:3:4:5:6:7:8", NULL },
    { AF_INET6, "a:b:c:d:e:f:1:2", NULL },
    { AF_INET6, "A:B:C:D:E:F:1:2", "a:b:c:d:e:f:1:2" },
    { AF_INET6, "::", "::" },
    { AF_INET6, "::12", "::12" },
    { AF_INET6, "::1:2:3", "::1:2:3" },
    { AF_INET6, "12::", "12::" },
    { AF_INET6, "1:2::", "1:2::" },
    { AF_INET6, "12:0:0:0:0:0:0:0", "12::" },
    { AF_INET6, "1:2:3::4:5", "1:2:3::4:5" },
    { AF_INET6, "::ffff:1.1.1.1", "::ffff:1.1.1.1" },
    { AF_INET6, "ffff::1.1.1.1", "ffff::101:101" },
    { AF_INET6, "::1.1.1.1", "::1.1.1.1" },
  };

  for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
    uint8_t addr[16];
    ASSERT_TRUE(inet_pton(tests[i].family, tests[i].input, addr))
        << "inet_pton failed for " << tests[i].input;
    const char* expected = tests[i].output ? tests[i].output : tests[i].input;
    char out_buffer[256];
    ASSERT_EQ(out_buffer,
              inet_ntop(tests[i].family, addr, out_buffer, sizeof(out_buffer)));
    ASSERT_STREQ(expected, out_buffer);
  }
}

TEST(SocketUtilityFunctions, Inet_pton_failure) {
  // All these are examples of strings that do not map
  // to IP address.  inet_pton returns 0 on failure.
  uint8_t addr[16];
  EXPECT_EQ(0, inet_pton(AF_INET, "127.127.12.24312", &addr));
  EXPECT_EQ(0, inet_pton(AF_INET, "127.127.12.24 ", &addr));
  EXPECT_EQ(0, inet_pton(AF_INET, "127.127.12.0.1", &addr));
  EXPECT_EQ(0, inet_pton(AF_INET, "127.127.12. 0", &addr));
  EXPECT_EQ(0, inet_pton(AF_INET, " 127.127.12.0", &addr));
  EXPECT_EQ(0, inet_pton(AF_INET, "127.127.12.0.", &addr));
  EXPECT_EQ(0, inet_pton(AF_INET, ".127.127.12.0", &addr));
  EXPECT_EQ(0, inet_pton(AF_INET, "127.127.12.0x0", &addr));

  EXPECT_EQ(0, inet_pton(AF_INET6, ":::", &addr));
  EXPECT_EQ(0, inet_pton(AF_INET6, "0:::0", &addr));
  EXPECT_EQ(0, inet_pton(AF_INET6, "0::0:0::1", &addr));
  EXPECT_EQ(0, inet_pton(AF_INET6, "0:0:0:0:0:0:1: 2", &addr));
  EXPECT_EQ(0, inet_pton(AF_INET6, " 0:0:0:0:0:0:1:2", &addr));
  EXPECT_EQ(0, inet_pton(AF_INET6, "0:0:0:0:0:0:1:2 ", &addr));
  EXPECT_EQ(0, inet_pton(AF_INET6, ":0:0:0:0:0:0:1:2", &addr));
  EXPECT_EQ(0, inet_pton(AF_INET6, "0:0:0:0:0:0:1:2:4", &addr));
  EXPECT_EQ(0, inet_pton(AF_INET6, "0:0:0:0:0:0:1:0.0.0.0", &addr));
  EXPECT_EQ(0, inet_pton(AF_INET6, "::0.0.0.0:1", &addr));
  EXPECT_EQ(0, inet_pton(AF_INET6, "::0.0.0.0.0", &addr));
  EXPECT_EQ(0, inet_pton(AF_INET6, "::1.2.3.4.5.6.7.8", &addr));
}

TEST(SocketUtilityFunctions, Ntohs) {
  uint8_t network_bytes[2] = { 0x22, 0x11 };
  uint16_t network_short;
  memcpy(&network_short, network_bytes, 2);
  uint16_t host_short = ntohs(network_short);
  EXPECT_EQ(host_short, 0x2211);
}

TEST(SocketUtilityFunctions, Ntohl) {
  uint8_t network_bytes[4] = { 0x44, 0x33, 0x22, 0x11 };
  uint32_t network_long;
  memcpy(&network_long, network_bytes, 4);
  uint32_t host_long = ntohl(network_long);
  EXPECT_EQ(host_long, 0x44332211);
}

#endif  // PROVIDES_SOCKETPAIR_API
