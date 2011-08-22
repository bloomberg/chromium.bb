// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "chrome/browser/net/network_stats.h"
#include "net/base/host_resolver.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/test_completion_callback.h"
#include "net/test/test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace chrome_browser_net {

class NetworkStatsTest : public PlatformTest {
 public:
  NetworkStatsTest() {}
 protected:
  virtual void TearDown() {
    // Flush the message loop to make application verifiers happy.
    message_loop_.RunAllPending();
  }
  MessageLoopForIO message_loop_;
};

class NetworkStatsTestUDP : public NetworkStatsTest {
 public:
  NetworkStatsTestUDP()
      : test_server_(net::TestServer::TYPE_UDP_ECHO,
                     FilePath(FILE_PATH_LITERAL("net/data"))) {
  }

 protected:
  void RunUDPEchoTest(int bytes) {
    TestCompletionCallback cb;

    scoped_ptr<net::MockHostResolver> host_resolver(
        new net::MockHostResolver());

    UDPStatsClient* udp_stats_client = new UDPStatsClient();
    EXPECT_TRUE(udp_stats_client->Start(host_resolver.get(),
                                        test_server_.host_port_pair(),
                                        bytes,
                                        &cb));
    int rv = cb.WaitForResult();
    // Check there were no errors during connect/write/read to echo UDP server.
    EXPECT_EQ(0, rv);
  }

  net::TestServer test_server_;
};

class NetworkStatsTestTCP : public NetworkStatsTest {
 public:
  NetworkStatsTestTCP()
      : test_server_(net::TestServer::TYPE_TCP_ECHO,
                     FilePath(FILE_PATH_LITERAL("net/data"))) {
  }

 protected:
  void RunTCPEchoTest(int bytes) {
    TestCompletionCallback cb;

    scoped_ptr<net::MockHostResolver> host_resolver(
        new net::MockHostResolver());

    TCPStatsClient* tcp_stats_client = new TCPStatsClient();
    EXPECT_TRUE(tcp_stats_client->Start(host_resolver.get(),
                                        test_server_.host_port_pair(),
                                        bytes,
                                        &cb));
    int rv = cb.WaitForResult();
    // Check there were no errors during connect/write/read to echo TCP server.
    EXPECT_EQ(0, rv);
  }

  net::TestServer test_server_;
};

TEST_F(NetworkStatsTestUDP, UDPEcho_50B_Of_Data) {
  ASSERT_TRUE(test_server_.Start());
  RunUDPEchoTest(50);
}

TEST_F(NetworkStatsTestUDP, UDPEcho_1K_Of_Data) {
  ASSERT_TRUE(test_server_.Start());
  RunUDPEchoTest(1024);
}

TEST_F(NetworkStatsTestTCP, TCPEcho_50B_Of_Data) {
  ASSERT_TRUE(test_server_.Start());
  RunTCPEchoTest(50);
}

TEST_F(NetworkStatsTestTCP, TCPEcho_1K_Of_Data) {
  ASSERT_TRUE(test_server_.Start());
  RunTCPEchoTest(1024);
}

}  // namespace chrome_browser_net
