// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_ack_notifier.h"

#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {
namespace {

class QuicAckNotifierTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    notifier_.reset(new QuicAckNotifier(&delegate_));

    sequence_numbers_.insert(26);
    sequence_numbers_.insert(99);
    sequence_numbers_.insert(1234);
    notifier_->AddSequenceNumbers(sequence_numbers_);
  }

  SequenceNumberSet sequence_numbers_;
  MockAckNotifierDelegate delegate_;
  scoped_ptr<QuicAckNotifier> notifier_;
};

// Should trigger callback when we receive acks for all the registered seqnums.
TEST_F(QuicAckNotifierTest, TriggerCallback) {
  EXPECT_CALL(delegate_, OnAckNotification()).Times(1);
  EXPECT_TRUE(notifier_->OnAck(sequence_numbers_));
}

// Should trigger callback when we receive acks for all the registered seqnums,
// even though they are interspersed with other seqnums.
TEST_F(QuicAckNotifierTest, TriggerCallbackInterspersed) {
  sequence_numbers_.insert(3);
  sequence_numbers_.insert(55);
  sequence_numbers_.insert(805);

  EXPECT_CALL(delegate_, OnAckNotification()).Times(1);
  EXPECT_TRUE(notifier_->OnAck(sequence_numbers_));
}

// Should trigger callback when we receive acks for all the registered seqnums,
// even though they are split over multiple calls to OnAck.
TEST_F(QuicAckNotifierTest, TriggerCallbackMultipleCalls) {
  SequenceNumberSet seqnums;
  seqnums.insert(26);
  EXPECT_CALL(delegate_, OnAckNotification()).Times(0);
  EXPECT_FALSE(notifier_->OnAck(seqnums));

  seqnums.clear();
  seqnums.insert(55);
  seqnums.insert(9001);
  seqnums.insert(99);
  EXPECT_CALL(delegate_, OnAckNotification()).Times(0);
  EXPECT_FALSE(notifier_->OnAck(seqnums));

  seqnums.clear();
  seqnums.insert(1234);
  EXPECT_CALL(delegate_, OnAckNotification()).Times(1);
  EXPECT_TRUE(notifier_->OnAck(seqnums));
}

// Should not trigger callback if we never provide all the seqnums.
TEST_F(QuicAckNotifierTest, DoesNotTrigger) {
  SequenceNumberSet different_seqnums;
  different_seqnums.insert(14);
  different_seqnums.insert(15);
  different_seqnums.insert(16);

  // Should not trigger callback as not all packets have been seen.
  EXPECT_CALL(delegate_, OnAckNotification()).Times(0);
  EXPECT_FALSE(notifier_->OnAck(different_seqnums));
}

// Should trigger even after updating sequence numbers and receiving ACKs for
// new sequeunce numbers.
TEST_F(QuicAckNotifierTest, UpdateSeqNums) {
  // Uninteresting sequeunce numbers shouldn't trigger callback.
  SequenceNumberSet seqnums;
  seqnums.insert(6);
  seqnums.insert(7);
  seqnums.insert(2000);
  EXPECT_CALL(delegate_, OnAckNotification()).Times(0);
  EXPECT_FALSE(notifier_->OnAck(seqnums));

  // Update a couple of the sequence numbers (i.e. retransmitted packets)
  notifier_->UpdateSequenceNumber(99, 3000);
  notifier_->UpdateSequenceNumber(1234, 3001);

  seqnums.clear();
  seqnums.insert(26);    // original, unchanged
  seqnums.insert(3000);  // updated
  seqnums.insert(3001);  // updated
  EXPECT_CALL(delegate_, OnAckNotification()).Times(1);
  EXPECT_TRUE(notifier_->OnAck(seqnums));
}

}  // namespace
}  // namespace test
}  // namespace net
