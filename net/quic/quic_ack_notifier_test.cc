// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_ack_notifier.h"

#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace net {
namespace test {
namespace {

class QuicAckNotifierTest : public ::testing::Test {
 protected:
  QuicAckNotifierTest() : zero_(QuicTime::Delta::Zero()) {}

  void SetUp() override {
    delegate_ = new MockAckNotifierDelegate;
    notifier_.reset(new QuicAckNotifier(delegate_));

    notifier_->OnSerializedPacket();
    notifier_->OnSerializedPacket();
    notifier_->OnSerializedPacket();
  }

  MockAckNotifierDelegate* delegate_;
  scoped_ptr<QuicAckNotifier> notifier_;
  QuicTime::Delta zero_;
};

// Should trigger callback when we receive acks for all the registered seqnums.
TEST_F(QuicAckNotifierTest, TriggerCallback) {
  EXPECT_CALL(*delegate_, OnAckNotification(0, 0, zero_)).Times(1);
  EXPECT_FALSE(notifier_->OnAck(zero_));
  EXPECT_FALSE(notifier_->OnAck(zero_));
  EXPECT_TRUE(notifier_->OnAck(zero_));
}

// Should not trigger callback if we never provide all the seqnums.
TEST_F(QuicAckNotifierTest, DoesNotTrigger) {
  // Should not trigger callback as not all packets have been seen.
  EXPECT_CALL(*delegate_, OnAckNotification(_, _, _)).Times(0);
  EXPECT_FALSE(notifier_->OnAck(zero_));
  EXPECT_FALSE(notifier_->OnAck(zero_));
}

// Should not trigger callback if we abandon all three packets.
TEST_F(QuicAckNotifierTest, AbandonDoesNotTrigger) {
  // Should not trigger callback as not all packets have been seen.
  EXPECT_CALL(*delegate_, OnAckNotification(_, _, _)).Times(0);
  EXPECT_FALSE(notifier_->OnPacketAbandoned());
  EXPECT_FALSE(notifier_->OnPacketAbandoned());
  EXPECT_TRUE(notifier_->OnPacketAbandoned());
}

// Should trigger even after updating sequence numbers and receiving ACKs for
// new sequeunce numbers.
TEST_F(QuicAckNotifierTest, UpdateSeqNums) {
  // Update a couple of the sequence numbers (i.e. retransmitted packets)
  notifier_->OnPacketRetransmitted(20);
  notifier_->OnPacketRetransmitted(3);

  EXPECT_CALL(*delegate_, OnAckNotification(2, 20 + 3, _)).Times(1);
  EXPECT_FALSE(notifier_->OnAck(zero_));  // original
  EXPECT_FALSE(notifier_->OnAck(zero_));  // updated
  EXPECT_TRUE(notifier_->OnAck(zero_));   // updated
}

// Make sure the delegate is called with the delta time from the last ACK.
TEST_F(QuicAckNotifierTest, DeltaTime) {
  const QuicTime::Delta first_delta = QuicTime::Delta::FromSeconds(5);
  const QuicTime::Delta second_delta = QuicTime::Delta::FromSeconds(33);
  const QuicTime::Delta third_delta = QuicTime::Delta::FromSeconds(10);

  EXPECT_CALL(*delegate_, OnAckNotification(0, 0, third_delta)).Times(1);
  EXPECT_FALSE(notifier_->OnAck(first_delta));
  EXPECT_FALSE(notifier_->OnAck(second_delta));
  EXPECT_TRUE(notifier_->OnAck(third_delta));
}

}  // namespace
}  // namespace test
}  // namespace net
