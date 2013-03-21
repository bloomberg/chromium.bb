// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "chrome/browser/net/network_stats.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mock_host_resolver.h"
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
    message_loop_.RunUntilIdle();
  }
  MessageLoopForIO message_loop_;
};

class NetworkStatsTestUDP : public NetworkStatsTest {
 public:
  NetworkStatsTestUDP()
      : test_server_(net::TestServer::TYPE_UDP_ECHO,
                     net::TestServer::kLocalhost,
                     base::FilePath(FILE_PATH_LITERAL("net/data"))) {
  }

 protected:
  void RunUDPEchoTest(int bytes, int packets, bool has_proxy) {
    net::TestCompletionCallback cb;

    scoped_ptr<net::MockHostResolver> host_resolver(
        new net::MockHostResolver());

    // We will use HISTOGRAM_PORT_MAX as the PortIndex enum for testing
    // purposes.
    NetworkStats* udp_stats_client = new NetworkStats();
    EXPECT_TRUE(udp_stats_client->Start(host_resolver.get(),
                                        test_server_.host_port_pair(),
                                        NetworkStats::HISTOGRAM_PORT_MAX,
                                        has_proxy,
                                        bytes,
                                        packets,
                                        cb.callback()));
    int rv = cb.WaitForResult();
    // Check there were no errors during connect/write/read to echo UDP server.
    EXPECT_EQ(0, rv);
  }

  net::TestServer test_server_;
};

TEST_F(NetworkStatsTestUDP, UDPEcho100BytesHasProxy) {
  ASSERT_TRUE(test_server_.Start());
  RunUDPEchoTest(100, 1, true);
}

TEST_F(NetworkStatsTestUDP, UDPEcho100BytesNoProxy) {
  ASSERT_TRUE(test_server_.Start());
  RunUDPEchoTest(100, 1, false);
}

TEST_F(NetworkStatsTestUDP, UDPEcho500BytesHasProxy) {
  ASSERT_TRUE(test_server_.Start());
  RunUDPEchoTest(500, 1, true);
}

TEST_F(NetworkStatsTestUDP, UDPEcho500BytesNoProxy) {
  ASSERT_TRUE(test_server_.Start());
  RunUDPEchoTest(500, 1, false);
}

TEST_F(NetworkStatsTestUDP, UDPEcho1KBytesHasProxy) {
  ASSERT_TRUE(test_server_.Start());
  RunUDPEchoTest(1024, 1, true);
}

TEST_F(NetworkStatsTestUDP, UDPEcho1KBytesNoProxy) {
  ASSERT_TRUE(test_server_.Start());
  RunUDPEchoTest(1024, 1, false);
}

TEST_F(NetworkStatsTestUDP, UDPEchoMultiplePacketsHasProxy) {
  ASSERT_TRUE(test_server_.Start());
  RunUDPEchoTest(1024, 4, true);
}

TEST_F(NetworkStatsTestUDP, UDPEchoMultiplePacketsNoProxy) {
  ASSERT_TRUE(test_server_.Start());
  RunUDPEchoTest(1024, 4, false);
}

TEST_F(NetworkStatsTestUDP, VerifyBytesInvalidHeaders) {
  const uint32 KBytesToSend = 100;
  const uint32 packets_to_send = 1;
  NetworkStats network_stats;
  net::TestCompletionCallback cb;
  network_stats.Initialize(KBytesToSend,
                           NetworkStats::HISTOGRAM_PORT_MAX,
                           true,
                           packets_to_send,
                           cb.callback());
  uint32 packet_number = network_stats.packet_number();
  network_stats.set_base_packet_number(packet_number);

  std::string message;
  EXPECT_EQ(NetworkStats::ZERO_LENGTH_ERROR,
            network_stats.VerifyBytes(message));

  std::string version = base::StringPrintf("%02d", 1);
  std::string invalid_checksum = base::StringPrintf("%010d", 0);
  std::string payload_size = base::StringPrintf("%07d", KBytesToSend);
  std::string invalid_key = base::StringPrintf("%06d", -1);
  std::string key_string = base::StringPrintf("%06d", 899999);
  std::string packet_number_string = base::StringPrintf("%010d", packet_number);
  std::string short_payload(packet_number_string +
      "1234567890123456789012345678901234567890123456789012345678901234567890");
  std::string long_payload(packet_number_string +
      "1234567890123456789012345678901234567890123456789012345678901234567890"
      "1234567890123456789012345678901234567890123456789012345678901234567890");

  message = version;
  EXPECT_EQ(NetworkStats::NO_CHECKSUM_ERROR,
            network_stats.VerifyBytes(message));

  message = version + invalid_checksum;
  EXPECT_EQ(NetworkStats::NO_PAYLOAD_SIZE_ERROR,
            network_stats.VerifyBytes(message));

  message = version + invalid_checksum + payload_size;
  EXPECT_EQ(NetworkStats::NO_KEY_ERROR, network_stats.VerifyBytes(message));

  message = version + invalid_checksum + payload_size + invalid_key;
  EXPECT_EQ(NetworkStats::NO_PAYLOAD_ERROR, network_stats.VerifyBytes(message));

  message = version + invalid_checksum + payload_size + invalid_key +
      short_payload;
  EXPECT_EQ(NetworkStats::INVALID_KEY_ERROR,
            network_stats.VerifyBytes(message));

  message = version + invalid_checksum + payload_size + key_string +
      short_payload;
  EXPECT_EQ(NetworkStats::TOO_SHORT_PAYLOAD,
            network_stats.VerifyBytes(message));

  message = version + invalid_checksum + payload_size + key_string +
      long_payload;
  EXPECT_EQ(NetworkStats::TOO_LONG_PAYLOAD, network_stats.VerifyBytes(message));
}

TEST_F(NetworkStatsTestUDP, VerifyBytesInvalidChecksum) {
  const uint32 KBytesToSend = 100;
  const uint32 packets_to_send = 1;
  NetworkStats network_stats;
  net::TestCompletionCallback cb;

  network_stats.Initialize(KBytesToSend,
                           NetworkStats::HISTOGRAM_PORT_MAX,
                           true,
                           packets_to_send,
                           cb.callback());
  uint32 packet_number = network_stats.packet_number();
  network_stats.set_base_packet_number(packet_number);

  std::string message;
  EXPECT_EQ(NetworkStats::ZERO_LENGTH_ERROR,
            network_stats.VerifyBytes(message));

  std::string version = base::StringPrintf("%02d", 1);
  std::string invalid_checksum = base::StringPrintf("%010d", 0);
  std::string payload_size = base::StringPrintf("%07d", KBytesToSend);
  std::string key_string = base::StringPrintf("%06d", 899999);
  std::string packet_number_string = base::StringPrintf("%010d", packet_number);

  scoped_refptr<net::IOBuffer> payload(new net::IOBuffer(KBytesToSend));

  // Copy the Packet number into the payload.
  size_t packet_number_size = packet_number_string.size();
  memcpy(payload->data(), packet_number_string.c_str(), packet_number_size);

  // Get random bytes to fill rest of the payload.
  network_stats.stream_.GetBytes(payload->data() + packet_number_size,
                                 KBytesToSend - packet_number_size);

  // Calculate checksum for the payload.
  char* payload_data = payload->data();
  uint32 sum = network_stats.GetChecksum(payload_data, KBytesToSend);
  std::string checksum = base::StringPrintf("%010d", sum);

  char encoded_data[KBytesToSend];
  memset(encoded_data, 0, KBytesToSend);
  const char* key = key_string.c_str();
  network_stats.Crypt(
      key, key_string.length(), payload_data, KBytesToSend, encoded_data);
  std::string encoded_payload(encoded_data, KBytesToSend);

  // Test invalid checksum error by sending wrong checksum.
  message = version + invalid_checksum + payload_size + key_string +
      encoded_payload;
  EXPECT_EQ(NetworkStats::INVALID_CHECKSUM, network_stats.VerifyBytes(message));
}

TEST_F(NetworkStatsTestUDP, VerifyBytesPreviousPacket) {
  const uint32 KBytesToSend = 100;
  const uint32 packets_to_send = 1;
  NetworkStats network_stats;
  net::TestCompletionCallback cb;

  network_stats.Initialize(KBytesToSend,
                           NetworkStats::HISTOGRAM_PORT_MAX,
                           true,
                           packets_to_send,
                           cb.callback());
  uint32 packet_number = network_stats.packet_number();
  network_stats.set_base_packet_number(packet_number);
  uint32 prev_packet_number = packet_number - 1;

  std::string message;
  std::string version = base::StringPrintf("%02d", 1);
  std::string payload_size = base::StringPrintf("%07d", KBytesToSend);
  std::string key_string = base::StringPrintf("%06d", 899999);
  std::string prev_packet_number_string =
      base::StringPrintf("%010d", prev_packet_number);

  // Copy the previous packet number into the payload.
  scoped_refptr<net::IOBuffer> payload(new net::IOBuffer(KBytesToSend));
  size_t packet_number_size = prev_packet_number_string.size();
  memcpy(payload->data(), prev_packet_number_string.c_str(),
         packet_number_size);

  // Get random bytes to fill rest of the payload.
  network_stats.stream_.GetBytes(payload->data() + packet_number_size,
                                 KBytesToSend - packet_number_size);

  // Calculate checksum for the previous packet number payload.
  char* payload_data = payload->data();
  uint32 sum = network_stats.GetChecksum(payload_data, KBytesToSend);
  std::string checksum = base::StringPrintf("%010d", sum);

  char encoded_data[KBytesToSend];
  memset(encoded_data, 0, KBytesToSend);
  const char* key = key_string.c_str();
  network_stats.Crypt(
      key, key_string.length(), payload_data, KBytesToSend, encoded_data);
  std::string encoded_payload(encoded_data, KBytesToSend);

  message = version + checksum + payload_size + key_string +  encoded_payload;
  EXPECT_EQ(NetworkStats::PREVIOUS_PACKET_NUMBER,
            network_stats.VerifyBytes(message));
  EXPECT_EQ(0u, network_stats.packets_received_mask());
}

TEST_F(NetworkStatsTestUDP, VerifyBytesInvalidPacketNumber) {
  const uint32 KBytesToSend = 100;
  const uint32 packets_to_send = 1;
  NetworkStats network_stats;
  net::TestCompletionCallback cb;

  network_stats.Initialize(KBytesToSend,
                           NetworkStats::HISTOGRAM_PORT_MAX,
                           true,
                           packets_to_send,
                           cb.callback());
  uint32 packet_number = network_stats.packet_number();
  network_stats.set_base_packet_number(packet_number);
  uint32 corrupt_packet_number = packet_number + 1;

  std::string message;
  std::string version = base::StringPrintf("%02d", 1);
  std::string payload_size = base::StringPrintf("%07d", KBytesToSend);
  std::string key_string = base::StringPrintf("%06d", 899999);
  std::string corrupt_packet_number_string =
      base::StringPrintf("%010d", corrupt_packet_number);

  scoped_refptr<net::IOBuffer> payload(new net::IOBuffer(KBytesToSend));
  size_t packet_number_size = corrupt_packet_number_string.size();
  memcpy(payload->data(), corrupt_packet_number_string.c_str(),
         packet_number_size);

  // Get random bytes to fill rest of the payload.
  network_stats.stream_.GetBytes(payload->data() + packet_number_size,
                                 KBytesToSend - packet_number_size);

  // Calculate checksum for the corrupt packet number payload.
  char* payload_data = payload->data();
  uint32 sum = network_stats.GetChecksum(payload_data, KBytesToSend);
  std::string checksum = base::StringPrintf("%010d", sum);

  char encoded_data[KBytesToSend];
  memset(encoded_data, 0, KBytesToSend);
  const char* key = key_string.c_str();
  network_stats.Crypt(
      key, key_string.length(), payload_data, KBytesToSend, encoded_data);
  std::string encoded_payload(encoded_data, KBytesToSend);
  message = version + checksum + payload_size + key_string + encoded_payload;
  EXPECT_EQ(NetworkStats::INVALID_PACKET_NUMBER,
            network_stats.VerifyBytes(message));
  EXPECT_EQ(0u, network_stats.packets_received_mask());
}

TEST_F(NetworkStatsTestUDP, VerifyBytesPatternChanged) {
  const uint32 KBytesToSend = 100;
  const uint32 packets_to_send = 1;
  NetworkStats network_stats;
  net::TestCompletionCallback cb;

  network_stats.Initialize(KBytesToSend,
                           NetworkStats::HISTOGRAM_PORT_MAX,
                           true,
                           packets_to_send,
                           cb.callback());
  uint32 packet_number = network_stats.packet_number();
  network_stats.set_base_packet_number(packet_number);

  std::string message;
  std::string version = base::StringPrintf("%02d", 1);
  std::string payload_size = base::StringPrintf("%07d", KBytesToSend);
  std::string key_string = base::StringPrintf("%06d", 899999);
  std::string packet_number_string = base::StringPrintf("%010d", packet_number);

  scoped_refptr<net::IOBuffer> payload(new net::IOBuffer(KBytesToSend));

  // Copy the Packet number into the payload.
  size_t packet_number_size = packet_number_string.size();
  memcpy(payload->data(), packet_number_string.c_str(), packet_number_size);

  // Get random bytes to fill rest of the payload.
  network_stats.stream_.GetBytes(payload->data() + packet_number_size,
                                 KBytesToSend - packet_number_size);

  // Calculate checksum for the payload.
  char* payload_data = payload->data();
  uint32 sum = network_stats.GetChecksum(payload_data, KBytesToSend);
  std::string checksum = base::StringPrintf("%010d", sum);

  const char* key = key_string.c_str();

  // Corrupt the randomly generated bytes in payload data.
  char corrupted_data_2[KBytesToSend];
  memcpy(corrupted_data_2, payload_data, KBytesToSend);
  char temp_char_1 = corrupted_data_2[packet_number_size];
  corrupted_data_2[packet_number_size] =
      corrupted_data_2[packet_number_size + 1];
  corrupted_data_2[packet_number_size + 1] = temp_char_1;
  char encoded_corrupted_data_2[KBytesToSend];
  memset(encoded_corrupted_data_2, 0, KBytesToSend);
  network_stats.Crypt(key,
                      key_string.length(),
                      corrupted_data_2,
                      KBytesToSend,
                      encoded_corrupted_data_2);
  std::string invalid_payload(encoded_corrupted_data_2, KBytesToSend);
  message = version + checksum + payload_size + key_string + invalid_payload;
  EXPECT_EQ(NetworkStats::PATTERN_CHANGED, network_stats.VerifyBytes(message));
  EXPECT_EQ(0u, network_stats.packets_received_mask());
}


TEST_F(NetworkStatsTestUDP, VerifyBytes) {
  const uint32 KBytesToSend = 100;
  const uint32 packets_to_send = 1;
  NetworkStats network_stats;
  net::TestCompletionCallback cb;

  network_stats.Initialize(KBytesToSend,
                           NetworkStats::HISTOGRAM_PORT_MAX,
                           true,
                           packets_to_send,
                           cb.callback());
  uint32 packet_number = network_stats.packet_number();
  network_stats.set_base_packet_number(packet_number);

  std::string message;
  std::string version = base::StringPrintf("%02d", 1);
  std::string payload_size = base::StringPrintf("%07d", KBytesToSend);
  std::string key_string = base::StringPrintf("%06d", 899999);
  std::string packet_number_string = base::StringPrintf("%010d", packet_number);

  scoped_refptr<net::IOBuffer> payload(new net::IOBuffer(KBytesToSend));

  // Copy the Packet number into the payload.
  size_t packet_number_size = packet_number_string.size();
  memcpy(payload->data(), packet_number_string.c_str(), packet_number_size);

  // Get random bytes to fill rest of the payload.
  network_stats.stream_.GetBytes(payload->data() + packet_number_size,
                                 KBytesToSend - packet_number_size);

  // Calculate checksum for the payload.
  char* payload_data = payload->data();
  uint32 sum = network_stats.GetChecksum(payload_data, KBytesToSend);
  std::string checksum = base::StringPrintf("%010d", sum);

  char encoded_data[KBytesToSend];
  memset(encoded_data, 0, KBytesToSend);
  const char* key = key_string.c_str();
  network_stats.Crypt(
      key, key_string.length(), payload_data, KBytesToSend, encoded_data);
  std::string encoded_payload(encoded_data, KBytesToSend);

  message = version + checksum + payload_size + key_string + encoded_payload;
  EXPECT_EQ(NetworkStats::SUCCESS, network_stats.VerifyBytes(message));
  EXPECT_EQ(1u, network_stats.packets_received_mask());
}

}  // namespace chrome_browser_net
