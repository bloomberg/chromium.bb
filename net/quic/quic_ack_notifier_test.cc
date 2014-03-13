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
  virtual void SetUp() {
    delegate_ = new MockAckNotifierDelegate;
    notifier_.reset(new QuicAckNotifier(delegate_));

    notifier_->AddSequenceNumber(26, 100);
    notifier_->AddSequenceNumber(99, 20);
    notifier_->AddSequenceNumber(1234, 3);
  }

  MockAckNotifierDelegate* delegate_;
  scoped_ptr<QuicAckNotifier> notifier_;
};

// Should trigger callback when we receive acks for all the registered seqnums.
TEST_F(QuicAckNotifierTest, TriggerCallback) {
  EXPECT_CALL(*delegate_, OnAckNotification(3, 123, 0, 0)).Times(1);
  EXPECT_FALSE(notifier_->OnAck(26));
  EXPECT_FALSE(notifier_->OnAck(99));
  EXPECT_TRUE(notifier_->OnAck(1234));
}

// Should not trigger callback if we never provide all the seqnums.
TEST_F(QuicAckNotifierTest, DoesNotTrigger) {
  // Should not trigger callback as not all packets have been seen.
  EXPECT_CALL(*delegate_, OnAckNotification(_, _, _, _)).Times(0);
  EXPECT_FALSE(notifier_->OnAck(26));
  EXPECT_FALSE(notifier_->OnAck(99));
}

// Should trigger even after updating sequence numbers and receiving ACKs for
// new sequeunce numbers.
TEST_F(QuicAckNotifierTest, UpdateSeqNums) {
  // Update a couple of the sequence numbers (i.e. retransmitted packets)
  notifier_->UpdateSequenceNumber(99, 3000);
  notifier_->UpdateSequenceNumber(1234, 3001);

  EXPECT_CALL(*delegate_, OnAckNotification(3, 123, 2, 20 + 3)).Times(1);
  EXPECT_FALSE(notifier_->OnAck(26));  // original
  EXPECT_FALSE(notifier_->OnAck(3000));  // updated
  EXPECT_TRUE(notifier_->OnAck(3001));  // updated
}

}  // namespace
}  // namespace test
}  // namespace net
