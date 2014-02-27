// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/tcp_loss_algorithm.h"

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util.h"
#include "net/quic/quic_unacked_packet_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {
namespace {

class TcpLossAlgorithmTest : public ::testing::Test {
 protected:
  TcpLossAlgorithmTest()
      : unacked_packets_(true),
        srtt_(QuicTime::Delta::FromMilliseconds(10)) { }

  void SendDataPacket(QuicPacketSequenceNumber sequence_number) {
    SerializedPacket packet(sequence_number, PACKET_1BYTE_SEQUENCE_NUMBER,
                            NULL, 0, new RetransmittableFrames());
    unacked_packets_.AddPacket(packet);
    unacked_packets_.SetPending(sequence_number, QuicTime::Zero(), 1000);
  }

  void VerifyLosses(QuicPacketSequenceNumber largest_observed,
                    QuicPacketSequenceNumber* losses_expected,
                    size_t num_losses) {
    SequenceNumberSet lost_packets =
        loss_algorithm_.DetectLostPackets(
            unacked_packets_, QuicTime::Zero(), largest_observed, srtt_);
    EXPECT_EQ(num_losses, lost_packets.size());
    for (size_t i = 0; i < num_losses; ++i) {
      EXPECT_TRUE(ContainsKey(lost_packets, losses_expected[i]));
    }
  }

  QuicUnackedPacketMap unacked_packets_;
  TCPLossAlgorithm loss_algorithm_;
  QuicTime::Delta srtt_;
};

TEST_F(TcpLossAlgorithmTest, NackRetransmit1Packet) {
  const size_t kNumSentPackets = 5;
  // Transmit 5 packets.
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    SendDataPacket(i);
  }
  // No loss on one ack.
  unacked_packets_.SetNotPending(2);
  unacked_packets_.NackPacket(1, 1);
  VerifyLosses(2, NULL, 0);
  // No loss on two acks.
  unacked_packets_.SetNotPending(3);
  unacked_packets_.NackPacket(1, 2);
  VerifyLosses(3, NULL, 0);
  // Loss on three acks.
  unacked_packets_.SetNotPending(4);
  unacked_packets_.NackPacket(1, 3);
  QuicPacketSequenceNumber lost[] = { 1 };
  VerifyLosses(4, lost, arraysize(lost));
}

// A stretch ack is an ack that covers more than 1 packet of previously
// unacknowledged data.
TEST_F(TcpLossAlgorithmTest, NackRetransmit1PacketWith1StretchAck) {
  const size_t kNumSentPackets = 10;
  // Transmit 10 packets.
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    SendDataPacket(i);
  }

  // Nack the first packet 3 times in a single StretchAck.
  unacked_packets_.NackPacket(1, 3);
  unacked_packets_.SetNotPending(2);
  unacked_packets_.SetNotPending(3);
  unacked_packets_.SetNotPending(4);
  QuicPacketSequenceNumber lost[] = { 1 };
  VerifyLosses(4, lost, arraysize(lost));
}

// Ack a packet 3 packets ahead, causing a retransmit.
TEST_F(TcpLossAlgorithmTest, NackRetransmit1PacketSingleAck) {
  const size_t kNumSentPackets = 10;
  // Transmit 10 packets.
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    SendDataPacket(i);
  }

  // Nack the first packet 3 times in an AckFrame with three missing packets.
  unacked_packets_.NackPacket(1, 3);
  unacked_packets_.NackPacket(2, 2);
  unacked_packets_.NackPacket(3, 1);
  unacked_packets_.SetNotPending(4);
  QuicPacketSequenceNumber lost[] = { 1 };
  VerifyLosses(4, lost, arraysize(lost));
}

TEST_F(TcpLossAlgorithmTest, EarlyRetransmit1Packet) {
  const size_t kNumSentPackets = 2;
  // Transmit 2 packets.
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    SendDataPacket(i);
  }
  // Early retransmit when the final packet gets acked and the first is nacked.
  unacked_packets_.SetNotPending(2);
  unacked_packets_.NackPacket(1, 1);
  QuicPacketSequenceNumber lost[] = { 1 };
  VerifyLosses(2, lost, arraysize(lost));
}

TEST_F(TcpLossAlgorithmTest, EarlyRetransmitAllPackets) {
  const size_t kNumSentPackets = 5;
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    SendDataPacket(i);
  }
  // Early retransmit when the final packet gets acked and the first 4 are
  // nacked multiple times via FACK.
  unacked_packets_.SetNotPending(kNumSentPackets);
  for (size_t i = 1; i < kNumSentPackets; ++i) {
    unacked_packets_.NackPacket(i, kNumSentPackets - i);
  }
  QuicPacketSequenceNumber lost[] = { 1, 2, 3, 4 };
  VerifyLosses(kNumSentPackets, lost, arraysize(lost));
}

TEST_F(TcpLossAlgorithmTest, DontEarlyRetransmitNeuteredPacket) {
  const size_t kNumSentPackets = 2;
  // Transmit 2 packets.
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    SendDataPacket(i);
  }
  // Early retransmit when the final packet gets acked and the first is nacked.
  unacked_packets_.SetNotPending(2);
  unacked_packets_.NackPacket(1, 1);
  unacked_packets_.NeuterPacket(1);
  VerifyLosses(2, NULL, 0);
}

}  // namespace
}  // namespace test
}  // namespace net
