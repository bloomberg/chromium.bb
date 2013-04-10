// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test of the full congestion control chain.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/congestion_control/quic_congestion_manager.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/test_tools/mock_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::max;

namespace net {
namespace test {

class QuicCongestionManagerPeer : public QuicCongestionManager {
 public:
  explicit QuicCongestionManagerPeer(const QuicClock* clock,
                                     CongestionFeedbackType congestion_type)
      : QuicCongestionManager(clock, congestion_type) {
  }
  using QuicCongestionManager::SentBandwidth;
  using QuicCongestionManager::BandwidthEstimate;
};

class QuicCongestionControlTest : public ::testing::Test {
 protected:
  QuicCongestionControlTest()
      : start_(clock_.ApproximateNow()) {
  }

  void SetUpCongestionType(CongestionFeedbackType congestion_type) {
    manager_.reset(new QuicCongestionManagerPeer(&clock_, congestion_type));
  }

  MockClock clock_;
  QuicTime start_;
  scoped_ptr<QuicCongestionManagerPeer> manager_;
};

TEST_F(QuicCongestionControlTest, FixedRateSenderAPI) {
  SetUpCongestionType(kFixRate);
  QuicCongestionFeedbackFrame congestion_feedback;
  congestion_feedback.type = kFixRate;
  congestion_feedback.fix_rate.bitrate = QuicBandwidth::FromKBytesPerSecond(30);
  manager_->OnIncomingQuicCongestionFeedbackFrame(congestion_feedback,
                                                  clock_.Now());
  EXPECT_TRUE(manager_->SentBandwidth(clock_.Now()).IsZero());
  EXPECT_TRUE(manager_->TimeUntilSend(
    clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA).IsZero());
  manager_->SentPacket(1, clock_.Now(), kMaxPacketSize, NOT_RETRANSMISSION);
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(40),
            manager_->TimeUntilSend(
                clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA));
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(35));
  EXPECT_EQ(QuicTime::Delta::Infinite(),
            manager_->TimeUntilSend(
                clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA));
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_EQ(QuicTime::Delta::Infinite(),
            manager_->TimeUntilSend(
                clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA));
}

TEST_F(QuicCongestionControlTest, FixedRatePacing) {
  SetUpCongestionType(kFixRate);
  QuicAckFrame ack;
  ack.received_info.largest_observed = 0;
  manager_->OnIncomingAckFrame(ack, clock_.Now());

  QuicCongestionFeedbackFrame feedback;
  feedback.type = kFixRate;
  feedback.fix_rate.bitrate = QuicBandwidth::FromKBytesPerSecond(100);
  manager_->OnIncomingQuicCongestionFeedbackFrame(feedback, clock_.Now());

  QuicTime acc_advance_time(QuicTime::Zero());
  for (QuicPacketSequenceNumber i = 1; i <= 100; ++i) {
    EXPECT_TRUE(manager_->TimeUntilSend(
        clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA).IsZero());
    manager_->SentPacket(i, clock_.Now(), kMaxPacketSize, NOT_RETRANSMISSION);
    QuicTime::Delta advance_time = manager_->TimeUntilSend(
        clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
    clock_.AdvanceTime(advance_time);
    acc_advance_time = acc_advance_time.Add(advance_time);
    // Ack the packet we sent.
    ack.received_info.largest_observed = max(
        i, ack.received_info.largest_observed);
    manager_->OnIncomingAckFrame(ack, clock_.Now());
  }
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(1200),
            acc_advance_time.Subtract(start_));
}

TEST_F(QuicCongestionControlTest, Pacing) {
  SetUpCongestionType(kFixRate);
  QuicAckFrame ack;
  ack.received_info.largest_observed = 0;
  manager_->OnIncomingAckFrame(ack, clock_.Now());

  QuicCongestionFeedbackFrame feedback;
  feedback.type = kFixRate;
  // Test a high bitrate (8Mbit/s) to trigger pacing.
  feedback.fix_rate.bitrate = QuicBandwidth::FromKBytesPerSecond(1000);
  manager_->OnIncomingQuicCongestionFeedbackFrame(feedback, clock_.Now());

  QuicTime acc_advance_time(QuicTime::Zero());
  for (QuicPacketSequenceNumber i = 1; i <= 100;) {
    EXPECT_TRUE(manager_->TimeUntilSend(
        clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA).IsZero());
    manager_->SentPacket(i++, clock_.Now(), kMaxPacketSize, NOT_RETRANSMISSION);
    EXPECT_TRUE(manager_->TimeUntilSend(
        clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA).IsZero());
    manager_->SentPacket(i++, clock_.Now(), kMaxPacketSize, NOT_RETRANSMISSION);
    QuicTime::Delta advance_time = manager_->TimeUntilSend(
        clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
    clock_.AdvanceTime(advance_time);
    acc_advance_time = acc_advance_time.Add(advance_time);
    // Ack the packets we sent.
    ack.received_info.largest_observed = max(
        i - 2, ack.received_info.largest_observed);
    manager_->OnIncomingAckFrame(ack, clock_.Now());
    ack.received_info.largest_observed = max(
        i - 1, ack.received_info.largest_observed);
    manager_->OnIncomingAckFrame(ack, clock_.Now());
  }
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(120),
            acc_advance_time.Subtract(start_));
}

// TODO(pwestin): add TCP tests.

// TODO(pwestin): add InterArrival tests.

}  // namespace test
}  // namespace net
