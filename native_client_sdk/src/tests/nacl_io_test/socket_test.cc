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
  SocketTest() {}

  void SetUp() {
    ki_init(&kp_);
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

TEST_F(SocketTest, Socket) {
  EXPECT_LT(ki_socket(AF_UNIX, SOCK_STREAM, 0), 0);
  EXPECT_EQ(errno, EAFNOSUPPORT);
  EXPECT_LT(ki_socket(AF_INET, SOCK_RAW, 0), 0);
  EXPECT_EQ(errno, EPROTONOSUPPORT);
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

// These utility functions are only used for newlib (glibc provides its own
// implementations of these functions).
#if !defined(__GLIBC__)

TEST(SocketUtilityFunctions, Hstrerror) {
  EXPECT_STREQ(hstrerror(2718),
               "Unknown error in gethostbyname: 2718.");
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

static struct in_addr generate_ipv4_addr(int tuple1, int tuple2,
                                         int tuple3, int tuple4) {
  unsigned char addr[4];
  addr[0] = static_cast<unsigned char>(tuple1);
  addr[1] = static_cast<unsigned char>(tuple2);
  addr[2] = static_cast<unsigned char>(tuple3);
  addr[3] = static_cast<unsigned char>(tuple4);
  struct in_addr real_addr;
  memcpy(&real_addr, addr, 4);
  return real_addr;
}

static struct in6_addr generate_ipv6_addr(int* tuples) {
  unsigned char addr[16];
  for (int i = 0; i < 8; i++) {
    addr[2*i] = (tuples[i] >> 8) & 0xFF;
    addr[2*i+1] = tuples[i] & 0xFF;
  }
  struct in6_addr real_addr;
  memcpy(&real_addr, addr, 16);
  return real_addr;
}

TEST(SocketUtilityFunctions, Inet_ntoa) {
  char* stringified_addr = inet_ntoa(generate_ipv4_addr(0,0,0,0));
  ASSERT_TRUE(NULL != stringified_addr);
  EXPECT_STREQ("0.0.0.0", stringified_addr);

  stringified_addr = inet_ntoa(generate_ipv4_addr(127,0,0,1));
  ASSERT_TRUE(NULL != stringified_addr);
  EXPECT_STREQ("127.0.0.1", stringified_addr);

  stringified_addr = inet_ntoa(generate_ipv4_addr(255,255,255,255));
  ASSERT_TRUE(NULL != stringified_addr);
  EXPECT_STREQ("255.255.255.255", stringified_addr);
}

TEST(SocketUtilityFunctions, Inet_ntop_ipv4) {
  char stringified_addr[INET_ADDRSTRLEN];

  struct in_addr real_addr = generate_ipv4_addr(0,0,0,0);
  EXPECT_TRUE(NULL != inet_ntop(AF_INET, &real_addr,
                                stringified_addr, INET_ADDRSTRLEN));
  EXPECT_STREQ("0.0.0.0", stringified_addr);

  real_addr = generate_ipv4_addr(127,0,0,1);
  EXPECT_TRUE(NULL != inet_ntop(AF_INET, &real_addr,
                                stringified_addr, INET_ADDRSTRLEN));
  EXPECT_STREQ("127.0.0.1", stringified_addr);

  real_addr = generate_ipv4_addr(255,255,255,255);
  EXPECT_TRUE(NULL != inet_ntop(AF_INET, &real_addr,
                                stringified_addr, INET_ADDRSTRLEN));
  EXPECT_STREQ("255.255.255.255", stringified_addr);
}

TEST(SocketUtilityFunctions, Inet_ntop_ipv6) {
  char stringified_addr[INET6_ADDRSTRLEN];

  {
    int addr_tuples[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    struct in6_addr real_addr = generate_ipv6_addr(addr_tuples);
    EXPECT_TRUE(NULL != inet_ntop(AF_INET6, &real_addr,
                                  stringified_addr, INET6_ADDRSTRLEN));
    EXPECT_STREQ("0:0:0:0:0:0:0:0", stringified_addr);
  }

  {
    int addr_tuples[8] = { 0x1234, 0xa, 0x12, 0x0000,
                                0x5678, 0x9abc, 0xdef, 0xffff };
    struct in6_addr real_addr = generate_ipv6_addr(addr_tuples);
    EXPECT_TRUE(NULL != inet_ntop(AF_INET6, &real_addr,
                                  stringified_addr, INET6_ADDRSTRLEN));
    EXPECT_STREQ("1234:a:12:0:5678:9abc:def:ffff", stringified_addr);
  }

  {
    int addr_tuples[8] = { 0xffff, 0xffff, 0xffff, 0xffff,
                                0xffff, 0xffff, 0xffff, 0xffff };
    struct in6_addr real_addr = generate_ipv6_addr(addr_tuples);
    EXPECT_TRUE(NULL != inet_ntop(AF_INET6, &real_addr,
                                  stringified_addr, INET6_ADDRSTRLEN));
    EXPECT_STREQ("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", stringified_addr);
  }
}

TEST(SocketUtilityFunctions, Inet_ntop_failure) {
  char addr_name[INET6_ADDRSTRLEN];
  int addr_tuples[8] = { 0xffff, 0xffff, 0xffff, 0xffff,
                              0xffff, 0xffff, 0xffff, 0xffff };
  struct in6_addr ipv6_addr = generate_ipv6_addr(addr_tuples);
  struct in_addr ipv4_addr = generate_ipv4_addr(255,255,255,255);

  EXPECT_TRUE(NULL == inet_ntop(AF_UNIX, &ipv6_addr,
                                addr_name, INET6_ADDRSTRLEN));
  EXPECT_EQ(errno, EAFNOSUPPORT);

  EXPECT_TRUE(NULL == inet_ntop(AF_INET, &ipv4_addr,
                                addr_name, INET_ADDRSTRLEN - 1));
  EXPECT_EQ(errno, ENOSPC);

  EXPECT_TRUE(NULL == inet_ntop(AF_INET6, &ipv6_addr,
                                addr_name, INET6_ADDRSTRLEN - 1));
  EXPECT_EQ(errno, ENOSPC);
}

TEST(SocketUtilityFunctions, Inet_ptoa) {
  struct {
    int family;
    const char* input;
    const char* output;
  } tests[] = {
    { AF_INET, "127.127.12.0", NULL },
    { AF_INET, "0.0.0.0", NULL },

    { AF_INET6, "0:0:0:0:0:0:0:0", NULL },
    { AF_INET6, "1234:5678:9abc:def0:1234:5678:9abc:def0", NULL },
    { AF_INET6, "1:2:3:4:5:6:7:8", NULL },
    { AF_INET6, "a:b:c:d:e:f:1:2", NULL },
    { AF_INET6, "A:B:C:D:E:F:1:2", "a:b:c:d:e:f:1:2" },
    { AF_INET6, "::", "0:0:0:0:0:0:0:0" },
    { AF_INET6, "::12", "0:0:0:0:0:0:0:12" },
    { AF_INET6, "::1:2:3", "0:0:0:0:0:1:2:3" },
    { AF_INET6, "12::", "12:0:0:0:0:0:0:0" },
    { AF_INET6, "1:2::", "1:2:0:0:0:0:0:0" },
    { AF_INET6, "::12:0:0:0:0:0:0:0", "12:0:0:0:0:0:0:0" },
    { AF_INET6, "1:2:3::4:5", "1:2:3:0:0:0:4:5" },
    { AF_INET6, "::1.1.1.1", "0:0:0:0:0:0:101:101" },
    { AF_INET6, "::ffff:1.1.1.1", "0:0:0:0:0:ffff:101:101" },
    { AF_INET6, "ffff::1.1.1.1", "ffff:0:0:0:0:0:101:101" },
    { AF_INET6, "::1.1.1.1", "0:0:0:0:0:0:101:101" },
  };

  for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
    uint8_t addr[16];
    EXPECT_TRUE(inet_pton(tests[i].family, tests[i].input, addr));
    const char* expected = tests[i].output ? tests[i].output : tests[i].input;
    char out_buffer[256];
    EXPECT_TRUE(
        inet_ntop(tests[i].family, addr, out_buffer, sizeof(out_buffer)));
    EXPECT_EQ(std::string(expected), std::string(out_buffer));
  }
}

TEST(SocketUtilityFunctions, Inet_ptoa_failure) {
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

#endif  // !defined(__GLIBC__)
#endif  // PROVIDES_SOCKETPAIR_API
