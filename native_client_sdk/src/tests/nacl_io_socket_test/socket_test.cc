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

// No error expected
#define ENONE 0
#define LOCAL_HOST 0x7F000001
#define PORT1 4006
#define PORT2 4007
#define ANY_PORT 0


namespace {
class SocketTest : public ::testing::Test {
 public:
  SocketTest() : sock1(0), sock2(0) {}

  ~SocketTest() {
    EXPECT_EQ(0, close(sock1));
    EXPECT_EQ(0, close(sock2));
  }

  void IP4ToSockAddr(uint32_t ip, uint16_t port, struct sockaddr_in* addr) {
    memset(addr, 0, sizeof(*addr));

    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    addr->sin_addr.s_addr = htonl(ip);
  }

  int Bind(int fd, uint32_t ip, uint16_t port) {
    sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);

    IP4ToSockAddr(ip, port, &addr);
    int err = bind(fd, (sockaddr*) &addr, addrlen);

    if (err == -1)
      return errno;
    return 0;
  }

  void SetupPorts() {
    EXPECT_EQ(Bind(sock1, LOCAL_HOST, 0), ENONE);
    EXPECT_EQ(Bind(sock2, LOCAL_HOST, 0), ENONE);
  }

 public:
  int sock1;
  int sock2;
};

class SocketTestUDP : public SocketTest {
 public:
  SocketTestUDP() {
    sock1 = socket(AF_INET, SOCK_DGRAM, 0);
    sock2 = socket(AF_INET, SOCK_DGRAM, 0);

    EXPECT_LT(-1, sock1);
    EXPECT_LT(-1, sock2);
  }
};

class SocketTestTCP : public SocketTest {
 public:
  SocketTestTCP() {
    sock1 = socket(AF_INET, SOCK_STREAM, 0);
    sock2 = socket(AF_INET, SOCK_STREAM, 0);

    EXPECT_LT(-1, sock1);
    EXPECT_LT(-1, sock2);
  }
};

}  // namespace

TEST(SocketTestSimple, Socket) {
  EXPECT_EQ(-1, socket(AF_UNIX, SOCK_STREAM, 0));
  EXPECT_EQ(errno, EAFNOSUPPORT);
  EXPECT_EQ(-1, socket(AF_INET, SOCK_RAW, 0));
  EXPECT_EQ(errno, EPROTONOSUPPORT);

  int sock1 = socket(AF_INET, SOCK_DGRAM, 0);
  EXPECT_NE(-1, sock1);

  int sock2 = socket(AF_INET6, SOCK_DGRAM, 0);
  EXPECT_NE(-1, sock2);

  int sock3 = socket(AF_INET, SOCK_STREAM, 0);
  EXPECT_NE(-1, sock3);

  int sock4 = socket(AF_INET6, SOCK_STREAM, 0);
  EXPECT_NE(-1, sock4);

  close(sock1);
  close(sock2);
  close(sock3);
  close(sock4);
}

TEST_F(SocketTestUDP, Bind) {
  // Bind away.
  EXPECT_EQ(Bind(sock1, LOCAL_HOST, PORT1), ENONE);

  // Invalid to rebind a socket.
  EXPECT_EQ(Bind(sock1, LOCAL_HOST, PORT1), EINVAL);

  // Addr in use.
  EXPECT_EQ(Bind(sock2, LOCAL_HOST, PORT1), EADDRINUSE);

  // Bind with a wildcard.
  EXPECT_EQ(Bind(sock2, LOCAL_HOST, ANY_PORT), ENONE);

  // Invalid to rebind after wildcard
  EXPECT_EQ(Bind(sock2, LOCAL_HOST, PORT1), EINVAL);

}

TEST_F(SocketTestUDP, SendRcv) {
  char outbuf[256];
  char inbuf[512];

  memset(outbuf, 1, sizeof(outbuf));
  memset(inbuf, 0, sizeof(inbuf));

  EXPECT_EQ(Bind(sock1, LOCAL_HOST, PORT1), ENONE);
  EXPECT_EQ(Bind(sock2, LOCAL_HOST, PORT2), ENONE);

  sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  IP4ToSockAddr(LOCAL_HOST, PORT2, &addr);

  int len1 =
     sendto(sock1, outbuf, sizeof(outbuf), 0, (sockaddr *) &addr, addrlen);
  EXPECT_EQ(sizeof(outbuf), len1);

  // Ensure the buffers are different
  EXPECT_NE(0, memcmp(outbuf, inbuf, sizeof(outbuf)));
  memset(&addr, 0, sizeof(addr));

  // Try to receive the previously sent packet
  int len2 =
    recvfrom(sock2, inbuf, sizeof(inbuf), 0, (sockaddr *) &addr, &addrlen);
  EXPECT_EQ(sizeof(outbuf), len2);
  EXPECT_EQ(sizeof(sockaddr_in), addrlen);
  EXPECT_EQ(PORT1, htons(addr.sin_port));

  // Now they should be the same
  EXPECT_EQ(0, memcmp(outbuf, inbuf, sizeof(outbuf)));
}

#if 0
TEST_F(SocketTestTCP, Connect) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  EXPECT_NE(-1, sock);

  sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);

  IP4ToSockAddr(LOCAL_HOST, PORT1, &addr);
  int err = connect(sock, (sockaddr*) &addr, addrlen);
  EXPECT_EQ(ENONE, err) << "Failed with errno: " << errno << "\n";
}
#endif

#endif  // PROVIDES_SOCKETPAIR_API
