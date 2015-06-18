// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_ack_notifier_manager.h"

#include "net/quic/quic_ack_notifier.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/test_tools/quic_ack_notifier_manager_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {
namespace {

// Test fixture for testing AckNotifierManager.  Instantiates a manager and
// provides shared code for adding notifiers and verifying the contents of the
// manager.
class QuicAckNotifierManagerTest : public ::testing::Test {
 protected:
  AckNotifierManager manager_;
  scoped_refptr<MockAckNotifierDelegate> delegate_;
  QuicTime::Delta zero_;

  QuicAckNotifierManagerTest() : zero_(QuicTime::Delta::Zero()) {
    delegate_ = new MockAckNotifierDelegate;
  }

  ~QuicAckNotifierManagerTest() override {}

  size_t CountPackets() const {
    return AckNotifierManagerPeer::GetNumberOfRegisteredPackets(&manager_);
  }

  // Add a mock packet with specified parameters.  The packet with given
  // sequence number must not exist in the map before.
  void AddPacket(QuicPacketSequenceNumber sequence_number,
                 bool retransmittable) {
    // Create a mock packet.
    RetransmittableFrames frames(ENCRYPTION_NONE);
    SerializedPacket packet(sequence_number, PACKET_4BYTE_SEQUENCE_NUMBER,
                            /*packet=*/nullptr,
                            /*entropy_hash=*/0,
                            retransmittable ? &frames : nullptr);

    // Create and register a notifier.  Normally, this would be created by
    // QuicPacketGenerator.
    QuicAckNotifier* notifier = new QuicAckNotifier(delegate_.get());
    packet.notifiers.push_back(notifier);

    // Ensure that exactly one packet is added.
    const size_t old_packet_count = CountPackets();

    // Actually add the packet.
    manager_.OnSerializedPacket(packet);

    // Ensure the change in the number of packets.
    EXPECT_EQ(old_packet_count + 1, CountPackets());
  }
};

// This test verifies that QuicAckNotifierManager can handle the trivial case of
// received packet notification.
TEST_F(QuicAckNotifierManagerTest, SimpleAck) {
  AddPacket(1, true);
  AddPacket(2, true);

  EXPECT_CALL(*delegate_, OnAckNotification(0, 0, zero_)).Times(2);
  manager_.OnPacketAcked(1, zero_);
  EXPECT_EQ(1u, CountPackets());
  manager_.OnPacketAcked(2, zero_);
  EXPECT_EQ(0u, CountPackets());
}

// This test verifies that the QuicAckNotifierManager behaves correctly when
// there are many retransmissions.
TEST_F(QuicAckNotifierManagerTest, RepeatedRetransmission) {
  AddPacket(1, true);

  const size_t packet_size = kDefaultMaxPacketSize;
  const size_t times_lost = 100;
  const size_t total_size_lost = packet_size * times_lost;
  const QuicPacketSequenceNumber last_packet = times_lost + 1;

  // Retransmit the packet many times.
  for (size_t sequence_number = 1; sequence_number < last_packet;
       sequence_number++) {
    manager_.OnPacketRetransmitted(sequence_number, sequence_number + 1,
                                   packet_size);
    EXPECT_EQ(1u, CountPackets());
  }

  // Finally get the packet acknowledged.
  EXPECT_CALL(*delegate_, OnAckNotification(times_lost, total_size_lost, zero_))
      .Times(1);
  manager_.OnPacketAcked(last_packet, zero_);
  EXPECT_EQ(0u, CountPackets());
}

}  // namespace
}  // namespace test
}  // namespace net
