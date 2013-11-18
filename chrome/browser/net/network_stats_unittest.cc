// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/net/network_stats.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/socket/socket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace chrome_browser_net {

class NetworkStatsTest : public PlatformTest {
 public:
  NetworkStatsTest() {}

 protected:

  virtual void SetUp() {
    net::NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    base::MessageLoop::current()->RunUntilIdle();
    mock_writes_.clear();
    mock_reads_.clear();
  }

  virtual void TearDown() {
    net::NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    // Empty the current queue.
    base::MessageLoop::current()->RunUntilIdle();
    PlatformTest::TearDown();
  }

  void CreateToken(uint64 timestamp_micros,
                   const string& hash,
                   ProbePacket_Token* token) {
    token->set_timestamp_micros(timestamp_micros);
    token->mutable_hash()->assign(hash);
  }

  // DeterministicMockData defines the exact sequence of the read/write
  // operations (specified by the last parameter of MockRead/MockWrite).
  // |io_mode_write| is the IO mode for writing only. Reading is always async.
  void MakeDeterministicMockData(uint32 max_tests,
                                 uint32 max_probe_packets,
                                 uint32 probe_bytes,
                                 net::IoMode io_mode_write) {
    // Only allow 0 or 1 test because the test 2 in NetworkStats is random.
    DCHECK_LT(max_tests, 2U);
    outputs_.resize(10);
    ProbePacket probe_packet;
    ProbeMessage probe_message;
    probe_message.SetPacketHeader(ProbePacket_Type_HELLO_REQUEST,
                                  &probe_packet);
    probe_packet.set_group_id(0);
    outputs_[0] = probe_message.MakeEncodedPacket(probe_packet);
    mock_writes_.clear();
    mock_writes_.push_back(net::MockWrite(
        io_mode_write, &outputs_[0][0], outputs_[0].size(), 0));
    // Add one probe_request.
    probe_packet = ProbePacket();  // Clear all content.
    ProbePacket_Token token;
    CreateToken(1L, "1010", &token);
    probe_message.GenerateProbeRequest(
        token, 1, probe_bytes, 0, max_probe_packets, &probe_packet);
    outputs_[1] = probe_message.MakeEncodedPacket(probe_packet);
    mock_writes_.push_back(net::MockWrite(
        io_mode_write, &outputs_[1][0], outputs_[1].size(), 2));

    inputs_.resize(10);
    mock_reads_.clear();
    // Add a hello reply.
    probe_packet = ProbePacket();  // Clear all content.
    probe_message.SetPacketHeader(ProbePacket_Type_HELLO_REPLY, &probe_packet);
    probe_packet.set_group_id(0);
    CreateToken(1L, "1010", probe_packet.mutable_token());
    inputs_[0] = probe_message.MakeEncodedPacket(probe_packet);
    mock_reads_.push_back(
        net::MockRead(net::ASYNC, &inputs_[0][0], inputs_[0].size(), 1));

    for (uint32 i = 0; i < max_probe_packets; ++i) {
      // Add a probe reply.
      probe_packet = ProbePacket();  // Clear all content.
      probe_message.SetPacketHeader(ProbePacket_Type_PROBE_REPLY,
                                    &probe_packet);
      int padding_size = probe_bytes - probe_packet.ByteSize() - 8;
      probe_packet.mutable_padding()->append(
          std::string(std::max(0, padding_size), 0));
      probe_packet.mutable_header()->set_checksum(0);
      probe_packet.set_group_id(1);
      probe_packet.set_packet_index(i);
      inputs_[1 + i] = probe_message.MakeEncodedPacket(probe_packet);
      mock_reads_.push_back(net::MockRead(
          net::ASYNC, &inputs_[1 + i][0], inputs_[1 + i].size(), 3 + i));
    }
  }

  // Test NetworkStats::Start(...) method.
  void TestStart(bool has_proxy,
                 uint32 max_tests,
                 uint32 max_probe_packets,
                 uint32 bytes,
                 net::IoMode io_mode) {
    net::DeterministicMockClientSocketFactory mock_socket_factory;
    MakeDeterministicMockData(max_tests, max_probe_packets, bytes, io_mode);
    net::DeterministicSocketData test_data(
        &mock_reads_[0], mock_reads_.size(),
        &mock_writes_[0], mock_writes_.size());
    mock_socket_factory.AddSocketDataProvider(&test_data);
    NetworkStats* udp_stats_client = new NetworkStats(&mock_socket_factory);
    udp_stats_client->maximum_tests_ = max_tests;
    udp_stats_client->maximum_sequential_packets_ = max_probe_packets;

    net::TestCompletionCallback cb;
    scoped_ptr<net::MockHostResolver> host_resolver(
        new net::MockHostResolver());
    net::HostPortPair host_port_pair;
    EXPECT_TRUE(udp_stats_client->Start(host_resolver.get(),
                                        host_port_pair,
                                        9999,
                                        has_proxy,
                                        bytes,
                                        bytes,
                                        cb.callback()));
    int num_packets_run = (max_tests + 1) * 2 + max_probe_packets - 1;
    test_data.RunFor(num_packets_run);
    int rv = cb.WaitForResult();
    // Check there were no errors during connect/write/read to echo UDP server.
    EXPECT_EQ(0, rv);
  }

  // Make one write and then |max_probe_packets| reads.
  void MakeDelayedMockData(NetworkStats::TestType test_type,
                           uint32 probe_bytes,
                           uint32 pacing_interval_micros,
                           uint32 max_probe_packets,
                           net::IoMode io_mode) {
    outputs_.resize(1);
    ProbePacket probe_packet;
    ProbeMessage probe_message;
    mock_writes_.clear();
    ProbePacket_Token token;
    CreateToken(2L, "2a2b", &token);
    switch (test_type) {
      case NetworkStats::PACKET_SIZE_TEST:
      case NetworkStats::NON_PACED_PACKET_TEST:
        pacing_interval_micros = 0;
        break;
      case NetworkStats::NAT_BIND_TEST:
        // For NAT_BIND_TEST, we always set this to 1000000 to avoid the
        // randomness in NetworkStats::SendProbeRequest() and to match
        // the value chosen in TestStartOneTest() below.
        pacing_interval_micros = 1000000;
        break;
      default: {}  // Do nothing here.
    }
    probe_message.GenerateProbeRequest(token,
                                       1,  // current_test_index_ = 1.
                                       probe_bytes,
                                       pacing_interval_micros,
                                       max_probe_packets,
                                       &probe_packet);
    outputs_[0] = probe_message.MakeEncodedPacket(probe_packet);
    mock_writes_.push_back(
        net::MockWrite(io_mode, &outputs_[0][0], outputs_[0].size()));

    inputs_.resize(max_probe_packets);
    mock_reads_.clear();
    for (uint32 i = 0; i < max_probe_packets; ++i) {
      // Add a probe reply.
      probe_packet = ProbePacket();  // Clear all content.
      probe_message.SetPacketHeader(ProbePacket_Type_PROBE_REPLY,
                                    &probe_packet);
      int padding_size = probe_bytes - probe_packet.ByteSize() - 8;
      probe_packet.mutable_padding()->append(
          std::string(std::max(0, padding_size), 0));
      probe_packet.mutable_header()->set_checksum(0);
      probe_packet.set_group_id(1);
      probe_packet.set_packet_index(i);
      inputs_[i] = probe_message.MakeEncodedPacket(probe_packet);
      mock_reads_.push_back(
          net::MockRead(io_mode, &inputs_[i][0], inputs_[i].size()));
    }
  }

  // Test NetworkStats::StartOneTest(...) method.
  void TestStartOneTest(bool has_proxy,
                        NetworkStats::TestType test_type,
                        uint32 bytes,
                        uint32 interval_micros,
                        uint32 max_probe_packets,
                        net::IoMode io_mode) {

    net::MockClientSocketFactory mock_socket_factory;
    MakeDelayedMockData(
        test_type, bytes, interval_micros, max_probe_packets, io_mode);
    net::DelayedSocketData test_data(1,
                                     &mock_reads_[0],
                                     mock_reads_.size(),
                                     &mock_writes_[0],
                                     mock_writes_.size());
    mock_socket_factory.AddSocketDataProvider(&test_data);
    NetworkStats* udp_stats_client = new NetworkStats(&mock_socket_factory);
    udp_stats_client->maximum_tests_ = 1;  // Only do one probe at a time.
    udp_stats_client->maximum_sequential_packets_ = max_probe_packets;
    udp_stats_client->maximum_NAT_packets_ = max_probe_packets;
    // For NAT_BIND_TEST, we always set this to 1 (second) to avoid the
    // randomness in NetworkStats::SendProbeRequest().
    udp_stats_client->maximum_NAT_idle_seconds_ = 1;
    udp_stats_client->start_test_after_connect_ = false;
    udp_stats_client->inter_arrival_time_ =
        base::TimeDelta::FromMicroseconds(interval_micros);
    CreateToken(2L, "2a2b", &udp_stats_client->token_);

    net::TestCompletionCallback cb;
    scoped_ptr<net::MockHostResolver> host_resolver(
        new net::MockHostResolver());
    net::HostPortPair host_port_pair;
    EXPECT_TRUE(udp_stats_client->Start(host_resolver.get(),
                                        host_port_pair,
                                        9999,
                                        has_proxy,
                                        bytes,
                                        bytes,
                                        cb.callback()));
    // Test need to be added after Start() because Start() will reset
    // test_sequence_
    udp_stats_client->test_sequence_.push_back(test_type);
    udp_stats_client->current_test_index_ = 1;
    // Wait for host resolving and check if there were no errors during
    // connect/write/read to UDP server.
    int rv = cb.WaitForResult();
    EXPECT_EQ(0, rv);
    udp_stats_client->ReadData();
    udp_stats_client->StartOneTest();
    rv = cb.WaitForResult();
    EXPECT_EQ(0, rv);
  }

  base::MessageLoopForIO message_loop_;
  std::vector<std::string> inputs_;
  std::vector<std::string> outputs_;
  std::vector<net::MockRead> mock_reads_;
  std::vector<net::MockWrite> mock_writes_;
};

TEST_F(NetworkStatsTest, ProbeTest100BHasProxyGetToken) {
  TestStart(true, 0, 1, 100, net::ASYNC);
}

TEST_F(NetworkStatsTest, ProbeTest500BHasNoProxyGetTokenSync) {
  TestStart(false, 0, 1, 500, net::SYNCHRONOUS);
}

TEST_F(NetworkStatsTest, ProbeTest100BHasNoProxyOneTest) {
  TestStart(false, 1, 1, 100, net::ASYNC);
}

TEST_F(NetworkStatsTest, ProbeTest100BHasNoProxyOneTestSync) {
  TestStart(false, 1, 1, 100, net::SYNCHRONOUS);
}

TEST_F(NetworkStatsTest, ProbeTest100BHasProxyOneTest) {
  TestStart(true, 1, 1, 100, net::ASYNC);
}

TEST_F(NetworkStatsTest, ProbeTest100BHasProxyOneTestSync) {
  TestStart(true, 1, 1, 100, net::SYNCHRONOUS);
}

TEST_F(NetworkStatsTest, ProbeTest500BHasProxyOneTest) {
  TestStart(true, 1, 1, 500, net::ASYNC);
}

TEST_F(NetworkStatsTest, ProbeTest500BHasNoProxyOneTestSync) {
  TestStart(false, 1, 1, 500, net::SYNCHRONOUS);
}

TEST_F(NetworkStatsTest, ProbeTest500BHasNoProxyOneTest) {
  TestStart(false, 1, 1, 500, net::ASYNC);
}

TEST_F(NetworkStatsTest, ProbeTest500BHasProxyOneTestSync) {
  TestStart(true, 1, 1, 500, net::SYNCHRONOUS);
}

TEST_F(NetworkStatsTest, ProbeTest1200BHasProxyOneTest) {
  TestStart(true, 1, 1, 1200, net::ASYNC);
}

TEST_F(NetworkStatsTest, ProbeTest1200BHasNoProxyOneTestSync) {
  TestStart(false, 1, 1, 1200, net::SYNCHRONOUS);
}

TEST_F(NetworkStatsTest, ProbeTest1200BHasNoProxyOneTest) {
  TestStart(false, 1, 1, 1200, net::ASYNC);
}

TEST_F(NetworkStatsTest, ProbeTest1200BHasProxyOneTestSync) {
  TestStart(true, 1, 1, 1200, net::SYNCHRONOUS);
}

TEST_F(NetworkStatsTest, ProbeTest100BHasNoProxyOneTestMultiPackets) {
  TestStart(false, 1, 4, 100, net::ASYNC);
}

TEST_F(NetworkStatsTest, ProbeTest1200BHasProxyOneTestMultiPacketsSync) {
  TestStart(true, 1, 4, 1200, net::SYNCHRONOUS);
}

TEST_F(NetworkStatsTest, StartNonPacedTest100BHasProxy) {
  TestStartOneTest(
      true, NetworkStats::NON_PACED_PACKET_TEST, 100, 0, 1, net::ASYNC);
}

TEST_F(NetworkStatsTest, StartNonPacedTest100BHasNoProxySync) {
  TestStartOneTest(
      false, NetworkStats::NON_PACED_PACKET_TEST, 100, 0, 1, net::SYNCHRONOUS);
}

TEST_F(NetworkStatsTest, StartNonPacedTest500BHasNoProxy) {
  TestStartOneTest(
      false, NetworkStats::NON_PACED_PACKET_TEST, 500, 3, 1, net::ASYNC);
}

TEST_F(NetworkStatsTest, StartNonPacedTest1200BHasProxySync) {
  TestStartOneTest(
      true, NetworkStats::NON_PACED_PACKET_TEST, 1200, 1, 1, net::SYNCHRONOUS);
}

TEST_F(NetworkStatsTest, StartNonPacedTest500BHasNoProxyMulti) {
  TestStartOneTest(
      false, NetworkStats::NON_PACED_PACKET_TEST, 500, 2, 3, net::ASYNC);
}

TEST_F(NetworkStatsTest, StartNonPacedTest1200BHasProxySyncMulti) {
  TestStartOneTest(
      true, NetworkStats::NON_PACED_PACKET_TEST, 1200, 1, 4, net::SYNCHRONOUS);
}

TEST_F(NetworkStatsTest, StartPacedTest100BHasProxy) {
  TestStartOneTest(
      true, NetworkStats::PACED_PACKET_TEST, 100, 0, 1, net::ASYNC);
}

TEST_F(NetworkStatsTest, StartPacedTest100BHasNoProxySync) {
  TestStartOneTest(
      false, NetworkStats::PACED_PACKET_TEST, 100, 0, 1, net::SYNCHRONOUS);
}

TEST_F(NetworkStatsTest, StartPacedTest500BHasNoProxy) {
  TestStartOneTest(
      false, NetworkStats::PACED_PACKET_TEST, 500, 3, 1, net::ASYNC);
}

TEST_F(NetworkStatsTest, StartPacedTest1200BHasProxySync) {
  TestStartOneTest(
      true, NetworkStats::PACED_PACKET_TEST, 1200, 1, 1, net::SYNCHRONOUS);
}

TEST_F(NetworkStatsTest, StartPacedTest500BHasNoProxyMulti) {
  TestStartOneTest(
      false, NetworkStats::PACED_PACKET_TEST, 500, 2, 3, net::ASYNC);
}

TEST_F(NetworkStatsTest, StartPacedTest1200BHasProxySyncMulti) {
  TestStartOneTest(
      true, NetworkStats::PACED_PACKET_TEST, 1200, 1, 4, net::SYNCHRONOUS);
}

TEST_F(NetworkStatsTest, StartNATBindTest100BHasProxy) {
  TestStartOneTest(true, NetworkStats::NAT_BIND_TEST, 100, 0, 1, net::ASYNC);
}

TEST_F(NetworkStatsTest, StartNATBindTest100BHasNoProxySync) {
  TestStartOneTest(
      false, NetworkStats::NAT_BIND_TEST, 100, 3, 1, net::SYNCHRONOUS);
}

TEST_F(NetworkStatsTest, StartNATBindTest500BHasNoProxy) {
  TestStartOneTest(false, NetworkStats::NAT_BIND_TEST, 500, 0, 2, net::ASYNC);
}

TEST_F(NetworkStatsTest, StartNATBindTest1200BHasProxySync) {
  TestStartOneTest(
      true, NetworkStats::NAT_BIND_TEST, 1200, 3, 2, net::SYNCHRONOUS);
}

TEST_F(NetworkStatsTest, StartPacketSizeTest1500BHasProxy) {
  TestStartOneTest(
      true, NetworkStats::PACKET_SIZE_TEST, 1500, 0, 1, net::ASYNC);
}

TEST_F(NetworkStatsTest, StartPacketSizeTest1500HasNoProxySync) {
  TestStartOneTest(
      false, NetworkStats::PACKET_SIZE_TEST, 1500, 0, 1, net::SYNCHRONOUS);
}

TEST_F(NetworkStatsTest, StartPacketSizeTest1500BHasNoProxy) {
  TestStartOneTest(
      false, NetworkStats::PACKET_SIZE_TEST, 1500, 0, 1, net::ASYNC);
}

}  // namespace chrome_browser_net
