// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Test for FixRate sender and receiver.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/congestion_control/fix_rate_receiver.h"
#include "net/quic/congestion_control/fix_rate_sender.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/quic_protocol.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

class FixRateSenderPeer : public FixRateSender {
 public:
  explicit FixRateSenderPeer(const QuicClock* clock)
      : FixRateSender(clock) {
  }
  using FixRateSender::AvailableCongestionWindow;
};

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
        sender_(new FixRateSenderPeer(&clock_)),
        receiver_(new FixRateReceiverPeer()) {
    // Make sure clock does not start at 0.
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(2));
  }
  const QuicTime::Delta rtt_;
  MockClock clock_;
  SendAlgorithmInterface::SentPacketsMap not_used_;
  scoped_ptr<FixRateSenderPeer> sender_;
  scoped_ptr<FixRateReceiverPeer> receiver_;
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
  sender_->OnIncomingQuicCongestionFeedbackFrame(feedback, not_used_);
  EXPECT_EQ(300000, sender_->BandwidthEstimate().ToBytesPerSecond());
  EXPECT_TRUE(sender_->TimeUntilSend(false).IsZero());
  EXPECT_EQ(kMaxPacketSize * 2,
            sender_->AvailableCongestionWindow());
  sender_->SentPacket(1, kMaxPacketSize, false);
  EXPECT_EQ(3000u - kMaxPacketSize,
            sender_->AvailableCongestionWindow());
  EXPECT_TRUE(sender_->TimeUntilSend(false).IsZero());
  sender_->SentPacket(2, kMaxPacketSize, false);
  sender_->SentPacket(3, 600, false);
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10),
            sender_->TimeUntilSend(false));
  EXPECT_EQ(0u, sender_->AvailableCongestionWindow());
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(2));
  EXPECT_EQ(QuicTime::Delta::Infinite(), sender_->TimeUntilSend(false));
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(8));
  sender_->OnIncomingAck(1, kMaxPacketSize, rtt_);
  sender_->OnIncomingAck(2, kMaxPacketSize, rtt_);
  sender_->OnIncomingAck(3, 600, rtt_);
  EXPECT_TRUE(sender_->TimeUntilSend(false).IsZero());
}

TEST_F(FixRateTest, FixRatePacing) {
  const QuicByteCount packet_size = 1200;
  const QuicBandwidth bitrate = QuicBandwidth::FromKBytesPerSecond(240);
  const int64 num_packets = 200;
  QuicCongestionFeedbackFrame feedback;
  receiver_->SetBitrate(QuicBandwidth::FromKBytesPerSecond(240));
  ASSERT_TRUE(receiver_->GenerateCongestionFeedback(&feedback));
  sender_->OnIncomingQuicCongestionFeedbackFrame(feedback, not_used_);
  QuicTime acc_advance_time(QuicTime::Zero());
  QuicPacketSequenceNumber sequence_number = 0;
  for (int64 i = 0; i < num_packets; i += 2) {
    EXPECT_TRUE(sender_->TimeUntilSend(false).IsZero());
    EXPECT_EQ(kMaxPacketSize * 2,
              sender_->AvailableCongestionWindow());
    sender_->SentPacket(sequence_number++, packet_size, false);
    EXPECT_TRUE(sender_->TimeUntilSend(false).IsZero());
    sender_->SentPacket(sequence_number++, packet_size, false);
    QuicTime::Delta advance_time = sender_->TimeUntilSend(false);
    clock_.AdvanceTime(advance_time);
    sender_->OnIncomingAck(sequence_number - 1, packet_size, rtt_);
    sender_->OnIncomingAck(sequence_number - 2, packet_size, rtt_);
    acc_advance_time = acc_advance_time.Add(advance_time);
  }
  EXPECT_EQ(num_packets * packet_size * 1000000 / bitrate.ToBytesPerSecond(),
            static_cast<uint64>(acc_advance_time.ToMicroseconds()));
}

}  // namespace test
}  // namespace net
