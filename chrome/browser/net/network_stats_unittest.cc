// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "chrome/browser/net/network_stats.h"
#include "net/base/host_resolver.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_errors.h"
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
                     net::TestServer::kLocalhost,
                     FilePath(FILE_PATH_LITERAL("net/data"))) {
  }

 protected:
  void RunUDPEchoTest(int bytes) {
    net::TestCompletionCallback cb;

    scoped_ptr<net::MockHostResolver> host_resolver(
        new net::MockHostResolver());

    // We will use HISTOGRAM_PORT_MAX as the PortIndex enum for testing
    // purposes.
    UDPStatsClient* udp_stats_client = new UDPStatsClient();
    EXPECT_TRUE(udp_stats_client->Start(host_resolver.get(),
                                        test_server_.host_port_pair(),
                                        NetworkStats::HISTOGRAM_PORT_MAX,
                                        bytes,
                                        cb.callback()));
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
                     net::TestServer::kLocalhost,
                     FilePath(FILE_PATH_LITERAL("net/data"))) {
  }

 protected:
  void RunTCPEchoTest(int bytes) {
    net::TestCompletionCallback cb;

    scoped_ptr<net::MockHostResolver> host_resolver(
        new net::MockHostResolver());

    // We will use HISTOGRAM_PORT_MAX as the PortIndex enum for testing
    // purposes.
    TCPStatsClient* tcp_stats_client = new TCPStatsClient();
    EXPECT_TRUE(tcp_stats_client->Start(host_resolver.get(),
                                        test_server_.host_port_pair(),
                                        NetworkStats::HISTOGRAM_PORT_MAX,
                                        bytes,
                                        cb.callback()));
    int rv = cb.WaitForResult();
    // Check there were no errors during connect/write/read to echo TCP server.
    EXPECT_EQ(0, rv);
  }

  net::TestServer test_server_;
};

TEST_F(NetworkStatsTestUDP, UDPEcho_100B_Of_Data) {
  ASSERT_TRUE(test_server_.Start());
  RunUDPEchoTest(100);
}

TEST_F(NetworkStatsTestUDP, UDPEcho_1K_Of_Data) {
  ASSERT_TRUE(test_server_.Start());
  RunUDPEchoTest(1024);
}

TEST_F(NetworkStatsTestTCP, TCPEcho_100B_Of_Data) {
  ASSERT_TRUE(test_server_.Start());
  RunTCPEchoTest(100);
}

TEST_F(NetworkStatsTestTCP, TCPEcho_1K_Of_Data) {
  ASSERT_TRUE(test_server_.Start());
  RunTCPEchoTest(1024);
}

TEST_F(NetworkStatsTestTCP, VerifyBytes) {
  static const uint32 KBytesToSend = 100;
  TCPStatsClient network_stats;
  net::TestCompletionCallback cb;
  network_stats.Initialize(KBytesToSend,
                           NetworkStats::HISTOGRAM_PORT_MAX,
                           cb.callback());

  std::string message;
  network_stats.set_encoded_message(message);
  EXPECT_EQ(NetworkStats::ZERO_LENGTH_ERROR, network_stats.VerifyBytes());

  std::string version = base::StringPrintf("%02d", 1);
  std::string invalid_checksum = base::StringPrintf("%010d", 0);
  std::string payload_size = base::StringPrintf("%07d", KBytesToSend);
  std::string invalid_key = base::StringPrintf("%06d", -1);
  std::string key_string = base::StringPrintf("%06d", 899999);
  std::string short_payload(
      "1234567890123456789012345678901234567890123456789012345678901234567890");
  std::string long_payload(
      "1234567890123456789012345678901234567890123456789012345678901234567890"
      "1234567890123456789012345678901234567890123456789012345678901234567890");

  network_stats.set_encoded_message(version);
  EXPECT_EQ(NetworkStats::NO_CHECKSUM_ERROR, network_stats.VerifyBytes());

  message = version + invalid_checksum;
  network_stats.set_encoded_message(message);
  EXPECT_EQ(NetworkStats::NO_PAYLOAD_SIZE_ERROR, network_stats.VerifyBytes());

  message = version + invalid_checksum + payload_size;
  network_stats.set_encoded_message(message);
  EXPECT_EQ(NetworkStats::NO_KEY_ERROR, network_stats.VerifyBytes());

  message = version + invalid_checksum + payload_size + invalid_key;
  network_stats.set_encoded_message(message);
  EXPECT_EQ(NetworkStats::NO_PAYLOAD_ERROR, network_stats.VerifyBytes());

  message = version + invalid_checksum + payload_size + invalid_key +
      short_payload;
  network_stats.set_encoded_message(message);
  EXPECT_EQ(NetworkStats::INVALID_KEY_ERROR, network_stats.VerifyBytes());

  message = version + invalid_checksum + payload_size + key_string +
      short_payload;
  network_stats.set_encoded_message(message);
  EXPECT_EQ(NetworkStats::TOO_SHORT_PAYLOAD, network_stats.VerifyBytes());

  message = version + invalid_checksum + payload_size + key_string +
      long_payload;
  network_stats.set_encoded_message(message);
  EXPECT_EQ(NetworkStats::TOO_LONG_PAYLOAD, network_stats.VerifyBytes());

  scoped_refptr<net::IOBuffer> io_buffer(new net::IOBuffer(KBytesToSend));
  network_stats.stream_.GetBytes(io_buffer->data(), KBytesToSend);
  char* decoded_data = io_buffer->data();
  uint32 sum = network_stats.GetChecksum(decoded_data, KBytesToSend);
  std::string checksum = base::StringPrintf("%010d", sum);

  char encoded_data[KBytesToSend + 1];
  memset(encoded_data, 0, KBytesToSend + 1);
  const char* key = key_string.c_str();
  network_stats.Crypt(
      key, key_string.length(), decoded_data, KBytesToSend, encoded_data);
  std::string payload(encoded_data, KBytesToSend);

  message = version + invalid_checksum + payload_size + key_string + payload;
  network_stats.set_encoded_message(message);
  EXPECT_EQ(NetworkStats::INVALID_CHECKSUM, network_stats.VerifyBytes());

  char temp_char_0 = decoded_data[0];
  decoded_data[0] = decoded_data[1];
  decoded_data[1] = temp_char_0;
  char encoded_data_2[KBytesToSend + 1];
  memset(encoded_data_2, 0, KBytesToSend + 1);
  network_stats.Crypt(
      key, key_string.length(), decoded_data, KBytesToSend, encoded_data_2);
  std::string pattern_changed_payload(encoded_data_2, KBytesToSend);

  message = version + checksum + payload_size + key_string +
      pattern_changed_payload;
  network_stats.set_encoded_message(message);
  EXPECT_EQ(NetworkStats::PATTERN_CHANGED, network_stats.VerifyBytes());

  message = version + checksum + payload_size + key_string + payload;
  network_stats.set_encoded_message(message);
  EXPECT_EQ(NetworkStats::SUCCESS, network_stats.VerifyBytes());
}

TEST_F(NetworkStatsTest, GetHistogramNames) {
  // Test TCP, large packet, success histogram name.
  std::string rtt_histogram_name;
  std::string status_histogram_name;
  NetworkStats::GetHistogramNames(NetworkStats::PROTOCOL_TCP,
                                  NetworkStats::PORT_53,
                                  1024,
                                  net::OK,
                                  &rtt_histogram_name,
                                  &status_histogram_name);
  EXPECT_EQ("NetConnectivity.TCP.Success.53.1K.RTT", rtt_histogram_name);
  EXPECT_EQ("NetConnectivity.TCP.Status.53.1K", status_histogram_name);

  // Test UDP, small packet, failure histogram name.
  std::string rtt_histogram_name1;
  std::string status_histogram_name1;
  NetworkStats::GetHistogramNames(NetworkStats::PROTOCOL_UDP,
                                  NetworkStats::PORT_6121,
                                  100,
                                  net::ERR_INVALID_ARGUMENT,
                                  &rtt_histogram_name1,
                                  &status_histogram_name1);
  EXPECT_EQ("", rtt_histogram_name1);
  EXPECT_EQ("NetConnectivity.UDP.Status.6121.100B", status_histogram_name1);
}

}  // namespace chrome_browser_net
