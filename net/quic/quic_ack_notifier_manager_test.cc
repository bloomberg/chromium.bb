// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_ack_notifier_manager.h"

#include "net/quic/quic_ack_notifier.h"
#include "net/quic/quic_flags.h"
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
  bool save_FLAGS_quic_no_ack_notifier_;

  QuicAckNotifierManagerTest() : zero_(QuicTime::Delta::Zero()) {
    delegate_ = new MockAckNotifierDelegate;
    // This test is obsolete once this flag is deprecated.
    save_FLAGS_quic_no_ack_notifier_ = FLAGS_quic_no_ack_notifier;
    FLAGS_quic_no_ack_notifier = false;
  }

  ~QuicAckNotifierManagerTest() override {
    FLAGS_quic_no_ack_notifier = save_FLAGS_quic_no_ack_notifier_;
  }

  size_t CountPackets() const {
    return AckNotifierManagerPeer::GetNumberOfRegisteredPackets(&manager_);
  }

  // Add a mock packet with specified parameters.  The packet with given
  // packet number must not exist in the map before.
  void AddPacket(QuicPacketNumber packet_number, bool retransmittable) {
    // Create a mock packet.
    RetransmittableFrames frames(ENCRYPTION_NONE);
    SerializedPacket packet(packet_number, PACKET_4BYTE_PACKET_NUMBER,
                            /*packet=*/nullptr,
                            /*entropy_hash=*/0,
                            retransmittable ? &frames : nullptr,
                            /*has_ack=*/false,
                            /*has_stop_waiting=*/false);

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
  AddPacket(1, false);
  AddPacket(2, true);

  EXPECT_CALL(*delegate_, OnAckNotification(0, 0, zero_)).Times(2);
  manager_.OnPacketAcked(1, zero_);
  EXPECT_EQ(1u, CountPackets());
  manager_.OnPacketAcked(2, zero_);
  EXPECT_EQ(0u, CountPackets());

  manager_.OnPacketRemoved(1);
  manager_.OnPacketRemoved(2);
  EXPECT_EQ(0u, CountPackets());
}

// This test verifies that QuicAckNotifierManager can correctly handle the case
// when some of the packets are lost, which causes retransmission and removal
// from the unacked packet map.
TEST_F(QuicAckNotifierManagerTest, AckWithLosses) {
  const size_t retransmitted_packet_size = kDefaultMaxPacketSize;

  // Here, we simulate the following scenario:
  // 1. We send packets 1 to 5, where only odd-numbered packets are
  //    retransmittable.
  // 2. The peer acks 1, 2 and 5, but not 3 and 4.
  // 3. We retransmit 3 as 6.
  // 4. We remove 1 and 2, since we no longer care about them.
  // 4. The peer acks 6.
  // 5. We remove packets 3 to 6.

  // Step 1: send five packets.
  AddPacket(1, true);
  AddPacket(2, false);
  AddPacket(3, true);
  AddPacket(4, false);
  AddPacket(5, true);

  // Step 2: handle acks from peer.
  EXPECT_CALL(*delegate_, OnAckNotification(0, 0, zero_)).Times(3);
  manager_.OnPacketAcked(1, zero_);
  EXPECT_EQ(4u, CountPackets());
  manager_.OnPacketAcked(2, zero_);
  EXPECT_EQ(3u, CountPackets());
  manager_.OnPacketAcked(5, zero_);
  EXPECT_EQ(2u, CountPackets());

  // Step 3: retransmit 3 as 6.
  manager_.OnPacketRetransmitted(3, 6, retransmitted_packet_size);
  EXPECT_EQ(2u, CountPackets());

  // Step 4: remove 1 and 2.
  manager_.OnPacketRemoved(1);
  manager_.OnPacketRemoved(2);
  EXPECT_EQ(2u, CountPackets());

  // Step 4: ack packet 6.
  EXPECT_CALL(*delegate_,
              OnAckNotification(1, retransmitted_packet_size, zero_)).Times(1);
  manager_.OnPacketAcked(6, zero_);
  EXPECT_EQ(1u, CountPackets());

  // Step 5: remove all packets.  This causes packet 4 to be dropped from the
  // map.
  manager_.OnPacketRemoved(3);
  manager_.OnPacketRemoved(4);
  manager_.OnPacketRemoved(5);
  manager_.OnPacketRemoved(6);
  EXPECT_EQ(0u, CountPackets());
}

// This test verifies that the QuicAckNotifierManager behaves correctly when
// there are many retransmissions.
TEST_F(QuicAckNotifierManagerTest, RepeatedRetransmission) {
  AddPacket(1, true);

  const size_t packet_size = kDefaultMaxPacketSize;
  const size_t times_lost = 100;
  const size_t total_size_lost = packet_size * times_lost;
  const QuicPacketNumber last_packet = times_lost + 1;

  // Retransmit the packet many times.
  for (size_t packet_number = 1; packet_number < last_packet; packet_number++) {
    manager_.OnPacketRetransmitted(packet_number, packet_number + 1,
                                   packet_size);
    EXPECT_EQ(1u, CountPackets());
  }

  // Remove all lost packets.
  for (QuicPacketNumber packet = 1; packet < last_packet; packet++) {
    manager_.OnPacketRemoved(packet);
  }
  EXPECT_EQ(1u, CountPackets());

  // Finally get the packet acknowledged.
  EXPECT_CALL(*delegate_, OnAckNotification(times_lost, total_size_lost, zero_))
      .Times(1);
  manager_.OnPacketAcked(last_packet, zero_);
  EXPECT_EQ(0u, CountPackets());

  // Remove the last packet.
  manager_.OnPacketRemoved(last_packet);
  EXPECT_EQ(0u, CountPackets());
}

}  // namespace
}  // namespace test
}  // namespace net
