// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test for FixRate sender and receiver.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/congestion_control/fix_rate_receiver.h"
#include "net/quic/congestion_control/fix_rate_sender.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/quic_protocol.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

class FixRateReceiverPeer : public FixRateReceiver {
 public:
  FixRateReceiverPeer()
      : FixRateReceiver() {
  }
  void SetBitrate(QuicBandwidth fix_rate) {
    FixRateReceiver::configured_rate_ = fix_rate;
  }
};

class FixRateTest : public ::testing::Test {
 protected:
  FixRateTest()
      : rtt_(QuicTime::Delta::FromMilliseconds(30)),
        unused_bandwidth_(QuicBandwidth::Zero()),
        sender_(new FixRateSender(&clock_)),
        receiver_(new FixRateReceiverPeer()),
        start_(clock_.Now()) {
    // Make sure clock does not start at 0.
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(2));
  }
  const QuicTime::Delta rtt_;
  const QuicBandwidth unused_bandwidth_;
  MockClock clock_;
  SendAlgorithmInterface::SentPacketsMap unused_packet_map_;
  scoped_ptr<FixRateSender> sender_;
  scoped_ptr<FixRateReceiverPeer> receiver_;
  const QuicTime start_;
};

TEST_F(FixRateTest, ReceiverAPI) {
  QuicCongestionFeedbackFrame feedback;
  QuicTime timestamp(QuicTime::Zero());
  receiver_->SetBitrate(QuicBandwidth::FromKBytesPerSecond(300));
  receiver_->RecordIncomingPacket(1, 1, timestamp, false);
  ASSERT_TRUE(receiver_->GenerateCongestionFeedback(&feedback));
  EXPECT_EQ(kFixRate, feedback.type);
  EXPECT_EQ(300000u, feedback.fix_rate.bitrate.ToBytesPerSecond());
}

TEST_F(FixRateTest, SenderAPI) {
  QuicCongestionFeedbackFrame feedback;
  feedback.type = kFixRate;
  feedback.fix_rate.bitrate = QuicBandwidth::FromKBytesPerSecond(300);
  sender_->OnIncomingQuicCongestionFeedbackFrame(feedback,  clock_.Now(),
      unused_bandwidth_, unused_packet_map_);
  EXPECT_EQ(300000, sender_->BandwidthEstimate().ToBytesPerSecond());
  EXPECT_TRUE(sender_->TimeUntilSend(
      clock_.Now(), NOT_RETRANSMISSION, NO_RETRANSMITTABLE_DATA).IsZero());
  sender_->SentPacket(clock_.Now(), 1, kMaxPacketSize, NOT_RETRANSMISSION);
  EXPECT_TRUE(sender_->TimeUntilSend(
      clock_.Now(), NOT_RETRANSMISSION, NO_RETRANSMITTABLE_DATA).IsZero());
  sender_->SentPacket(clock_.Now(), 2, kMaxPacketSize, NOT_RETRANSMISSION);
  sender_->SentPacket(clock_.Now(), 3, 600, NOT_RETRANSMISSION);
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10), sender_->TimeUntilSend(
      clock_.Now(), NOT_RETRANSMISSION, NO_RETRANSMITTABLE_DATA));
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(2));
  EXPECT_EQ(QuicTime::Delta::Infinite(), sender_->TimeUntilSend(
      clock_.Now(), NOT_RETRANSMISSION, NO_RETRANSMITTABLE_DATA));
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(8));
  sender_->OnIncomingAck(1, kMaxPacketSize, rtt_);
  sender_->OnIncomingAck(2, kMaxPacketSize, rtt_);
  sender_->OnIncomingAck(3, 600, rtt_);
  EXPECT_TRUE(sender_->TimeUntilSend(
      clock_.Now(), NOT_RETRANSMISSION, NO_RETRANSMITTABLE_DATA).IsZero());
}

TEST_F(FixRateTest, FixRatePacing) {
  const QuicByteCount packet_size = 1200;
  const QuicBandwidth bitrate = QuicBandwidth::FromKBytesPerSecond(240);
  const int64 num_packets = 200;
  QuicCongestionFeedbackFrame feedback;
  receiver_->SetBitrate(QuicBandwidth::FromKBytesPerSecond(240));
  ASSERT_TRUE(receiver_->GenerateCongestionFeedback(&feedback));
  sender_->OnIncomingQuicCongestionFeedbackFrame(feedback, clock_.Now(),
      unused_bandwidth_, unused_packet_map_);
  QuicTime acc_advance_time(QuicTime::Zero());
  QuicPacketSequenceNumber sequence_number = 0;
  for (int i = 0; i < num_packets; i += 2) {
    EXPECT_TRUE(sender_->TimeUntilSend(
        clock_.Now(), NOT_RETRANSMISSION, NO_RETRANSMITTABLE_DATA).IsZero());
    sender_->SentPacket(clock_.Now(), sequence_number++, packet_size,
                        NOT_RETRANSMISSION);
    EXPECT_TRUE(sender_->TimeUntilSend(
        clock_.Now(), NOT_RETRANSMISSION, NO_RETRANSMITTABLE_DATA).IsZero());
    sender_->SentPacket(clock_.Now(), sequence_number++, packet_size,
                        NOT_RETRANSMISSION);
    QuicTime::Delta advance_time = sender_->TimeUntilSend(
        clock_.Now(), NOT_RETRANSMISSION, NO_RETRANSMITTABLE_DATA);
    clock_.AdvanceTime(advance_time);
    sender_->OnIncomingAck(sequence_number - 1, packet_size, rtt_);
    sender_->OnIncomingAck(sequence_number - 2, packet_size, rtt_);
    acc_advance_time = acc_advance_time.Add(advance_time);
  }
  EXPECT_EQ(num_packets * packet_size * 1000000 / bitrate.ToBytesPerSecond(),
            static_cast<uint64>(acc_advance_time.Subtract(start_)
                                .ToMicroseconds()));
}

}  // namespace test
}  // namespace net
