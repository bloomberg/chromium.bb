// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/tcp_cubic_sender.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/congestion_control/tcp_receiver.h"
#include "net/quic/test_tools/mock_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

const uint32 kDefaultWindowTCP = 10 * kDefaultTCPMSS;

// TODO(ianswett): Remove 10000 once b/10075719 is fixed.
const QuicTcpCongestionWindow kDefaultMaxCongestionWindowTCP = 10000;

class TcpCubicSenderPeer : public TcpCubicSender {
 public:
  TcpCubicSenderPeer(const QuicClock* clock,
                     bool reno,
                     QuicTcpCongestionWindow max_tcp_congestion_window)
      : TcpCubicSender(clock, reno, max_tcp_congestion_window) {
  }

  QuicTcpCongestionWindow congestion_window() {
    return congestion_window_;
  }

  using TcpCubicSender::AvailableSendWindow;
  using TcpCubicSender::SendWindow;
  using TcpCubicSender::AckAccounting;
};

class TcpCubicSenderTest : public ::testing::Test {
 protected:
  TcpCubicSenderTest()
      : rtt_(QuicTime::Delta::FromMilliseconds(60)),
        one_ms_(QuicTime::Delta::FromMilliseconds(1)),
        sender_(new TcpCubicSenderPeer(&clock_, true,
                                       kDefaultMaxCongestionWindowTCP)),
        receiver_(new TcpReceiver()),
        sequence_number_(1),
        acked_sequence_number_(0) {
  }

  void SendAvailableSendWindow() {
    QuicByteCount bytes_to_send = sender_->AvailableSendWindow();
    while (bytes_to_send > 0) {
      QuicByteCount bytes_in_packet = std::min(kDefaultTCPMSS, bytes_to_send);
      sender_->OnPacketSent(clock_.Now(), sequence_number_++, bytes_in_packet,
                            NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
      bytes_to_send -= bytes_in_packet;
      if (bytes_to_send > 0) {
        EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(), NOT_RETRANSMISSION,
                        HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());
      }
    }
  }
  // Normal is that TCP acks every other segment.
  void AckNPackets(int n) {
    for (int i = 0; i < n; ++i) {
      acked_sequence_number_++;
      sender_->OnPacketAcked(acked_sequence_number_, kDefaultTCPMSS, rtt_);
    }
    clock_.AdvanceTime(one_ms_);  // 1 millisecond.
  }

  const QuicTime::Delta rtt_;
  const QuicTime::Delta one_ms_;
  MockClock clock_;
  SendAlgorithmInterface::SentPacketsMap not_used_;
  scoped_ptr<TcpCubicSenderPeer> sender_;
  scoped_ptr<TcpReceiver> receiver_;
  QuicPacketSequenceNumber sequence_number_;
  QuicPacketSequenceNumber acked_sequence_number_;
};

TEST_F(TcpCubicSenderTest, SimpleSender) {
  QuicCongestionFeedbackFrame feedback;
  // At startup make sure we are at the default.
  EXPECT_EQ(kDefaultWindowTCP, sender_->AvailableSendWindow());
  EXPECT_EQ(kDefaultWindowTCP, sender_->GetCongestionWindow());
  // At startup make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());
  // Get default QuicCongestionFeedbackFrame from receiver.
  ASSERT_TRUE(receiver_->GenerateCongestionFeedback(&feedback));
  sender_->OnIncomingQuicCongestionFeedbackFrame(feedback, clock_.Now(),
                                                 not_used_);
  // Make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());
  // And that window is un-affected.
  EXPECT_EQ(kDefaultWindowTCP, sender_->AvailableSendWindow());
  EXPECT_EQ(kDefaultWindowTCP, sender_->GetCongestionWindow());

  // A retransmit should always return 0.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NACK_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());
}

TEST_F(TcpCubicSenderTest, ExponentialSlowStart) {
  const int kNumberOfAck = 20;
  QuicCongestionFeedbackFrame feedback;
  // At startup make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());
  // Get default QuicCongestionFeedbackFrame from receiver.
  ASSERT_TRUE(receiver_->GenerateCongestionFeedback(&feedback));
  sender_->OnIncomingQuicCongestionFeedbackFrame(feedback, clock_.Now(),
                                                 not_used_);
  // Make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());

  for (int n = 0; n < kNumberOfAck; ++n) {
    // Send our full send window.
    SendAvailableSendWindow();
    AckNPackets(2);
  }
  QuicByteCount bytes_to_send = sender_->SendWindow();
  EXPECT_EQ(kDefaultWindowTCP + kDefaultTCPMSS * 2 * kNumberOfAck,
            bytes_to_send);
}

TEST_F(TcpCubicSenderTest, SlowStartAckTrain) {
  // Make sure that we fall out of slow start when we send ACK train longer
  // than half the RTT, in this test case 30ms, which is more than 30 calls to
  // Ack2Packets in one round.
  // Since we start at 10 packet first round will be 5 second round 10 etc
  // Hence we should pass 30 at 65 = 5 + 10 + 20 + 30
  const int kNumberOfAck = 65;
  QuicCongestionFeedbackFrame feedback;
  // At startup make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());
  // Get default QuicCongestionFeedbackFrame from receiver.
  ASSERT_TRUE(receiver_->GenerateCongestionFeedback(&feedback));
  sender_->OnIncomingQuicCongestionFeedbackFrame(feedback, clock_.Now(),
                                                 not_used_);
  // Make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());

  for (int n = 0; n < kNumberOfAck; ++n) {
    // Send our full send window.
    SendAvailableSendWindow();
    AckNPackets(2);
  }
  QuicByteCount expected_send_window =
      kDefaultWindowTCP + (kDefaultTCPMSS * 2 * kNumberOfAck);
  EXPECT_EQ(expected_send_window, sender_->SendWindow());
  EXPECT_EQ(expected_send_window, sender_->GetCongestionWindow());
  // We should now have fallen out of slow start.
  SendAvailableSendWindow();
  AckNPackets(2);
  expected_send_window += kDefaultTCPMSS;
  EXPECT_EQ(expected_send_window, sender_->SendWindow());

  // Testing Reno phase.
  // We should need 141(65*2+1+10) ACK:ed packets before increasing window by
  // one.
  for (int m = 0; m < 70; ++m) {
    SendAvailableSendWindow();
    AckNPackets(2);
    EXPECT_EQ(expected_send_window, sender_->SendWindow());
  }
  SendAvailableSendWindow();
  AckNPackets(2);
  expected_send_window += kDefaultTCPMSS;
  EXPECT_EQ(expected_send_window, sender_->SendWindow());
}

TEST_F(TcpCubicSenderTest, SlowStartPacketLoss) {
  // Make sure that we fall out of slow start when we encounter a packet loss.
  const int kNumberOfAck = 10;
  QuicCongestionFeedbackFrame feedback;
  // At startup make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());
  // Get default QuicCongestionFeedbackFrame from receiver.
  ASSERT_TRUE(receiver_->GenerateCongestionFeedback(&feedback));
  sender_->OnIncomingQuicCongestionFeedbackFrame(feedback, clock_.Now(),
                                                 not_used_);
  // Make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());

  for (int i = 0; i < kNumberOfAck; ++i) {
    // Send our full send window.
    SendAvailableSendWindow();
    AckNPackets(2);
  }
  SendAvailableSendWindow();
  QuicByteCount expected_send_window = kDefaultWindowTCP +
      (kDefaultTCPMSS * 2 * kNumberOfAck);
  EXPECT_EQ(expected_send_window, sender_->SendWindow());

  sender_->OnPacketLost(acked_sequence_number_ + 1, clock_.Now());

  // Make sure that we should not send right now.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(), NOT_RETRANSMISSION,
      HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsInfinite());

  // We should now have fallen out of slow start.
  // We expect window to be cut in half.
  expected_send_window /= 2;
  EXPECT_EQ(expected_send_window, sender_->SendWindow());

  // Testing Reno phase.
  // We need to ack half of the pending packet before we can send again.
  int number_of_packets_in_window = expected_send_window / kDefaultTCPMSS;
  AckNPackets(number_of_packets_in_window);
  EXPECT_EQ(expected_send_window, sender_->SendWindow());
  EXPECT_EQ(0u, sender_->AvailableSendWindow());

  AckNPackets(1);
  expected_send_window += kDefaultTCPMSS;
  number_of_packets_in_window++;
  EXPECT_EQ(expected_send_window, sender_->SendWindow());

  // We should need number_of_packets_in_window ACK:ed packets before
  // increasing window by one.
  for (int k = 0; k < number_of_packets_in_window; ++k) {
    SendAvailableSendWindow();
    AckNPackets(1);
    EXPECT_EQ(expected_send_window, sender_->SendWindow());
  }
  SendAvailableSendWindow();
  AckNPackets(1);
  expected_send_window += kDefaultTCPMSS;
  EXPECT_EQ(expected_send_window, sender_->SendWindow());
}

TEST_F(TcpCubicSenderTest, RTOCongestionWindow) {
  EXPECT_EQ(kDefaultWindowTCP, sender_->SendWindow());

  // Expect the window to decrease to the minimum once the RTO fires.
  sender_->OnRetransmissionTimeout();
  EXPECT_EQ(2 * kDefaultTCPMSS, sender_->SendWindow());
}

TEST_F(TcpCubicSenderTest, RetransmissionDelay) {
  const int64 kRttMs = 10;
  const int64 kDeviationMs = 3;
  EXPECT_EQ(QuicTime::Delta::Zero(), sender_->RetransmissionDelay());

  sender_->AckAccounting(QuicTime::Delta::FromMilliseconds(kRttMs));

  // Initial value is to set the median deviation to half of the initial
  // rtt, the median in then multiplied by a factor of 4 and finally the
  // smoothed rtt is added which is the initial rtt.
  QuicTime::Delta expected_delay =
      QuicTime::Delta::FromMilliseconds(kRttMs + kRttMs / 2 * 4);
  EXPECT_EQ(expected_delay, sender_->RetransmissionDelay());

  for (int i = 0; i < 100; ++i) {
    // Run to make sure that we converge.
    sender_->AckAccounting(
        QuicTime::Delta::FromMilliseconds(kRttMs + kDeviationMs));
    sender_->AckAccounting(
        QuicTime::Delta::FromMilliseconds(kRttMs - kDeviationMs));
  }
  expected_delay = QuicTime::Delta::FromMilliseconds(kRttMs + kDeviationMs * 4);

  EXPECT_NEAR(kRttMs, sender_->SmoothedRtt().ToMilliseconds(), 1);
  EXPECT_NEAR(expected_delay.ToMilliseconds(),
              sender_->RetransmissionDelay().ToMilliseconds(),
              1);
  EXPECT_EQ(static_cast<int64>(
                sender_->GetCongestionWindow() * kNumMicrosPerSecond /
                sender_->SmoothedRtt().ToMicroseconds()),
            sender_->BandwidthEstimate().ToBytesPerSecond());
}

TEST_F(TcpCubicSenderTest, SlowStartMaxSendWindow) {
  const QuicTcpCongestionWindow kMaxCongestionWindowTCP = 50;
  const int kNumberOfAck = 100;
  sender_.reset(
      new TcpCubicSenderPeer(&clock_, false, kMaxCongestionWindowTCP));

  QuicCongestionFeedbackFrame feedback;
  // At startup make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());
  // Get default QuicCongestionFeedbackFrame from receiver.
  ASSERT_TRUE(receiver_->GenerateCongestionFeedback(&feedback));
  sender_->OnIncomingQuicCongestionFeedbackFrame(feedback, clock_.Now(),
                                                 not_used_);
  // Make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());

  for (int i = 0; i < kNumberOfAck; ++i) {
    // Send our full send window.
    SendAvailableSendWindow();
    AckNPackets(2);
  }
  QuicByteCount expected_send_window =
      kMaxCongestionWindowTCP * kDefaultTCPMSS;
  EXPECT_EQ(expected_send_window, sender_->SendWindow());
}

TEST_F(TcpCubicSenderTest, TcpRenoMaxCongestionWindow) {
  const QuicTcpCongestionWindow kMaxCongestionWindowTCP = 50;
  const int kNumberOfAck = 1000;
  sender_.reset(
      new TcpCubicSenderPeer(&clock_, true, kMaxCongestionWindowTCP));

  QuicCongestionFeedbackFrame feedback;
  // At startup make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());
  // Get default QuicCongestionFeedbackFrame from receiver.
  ASSERT_TRUE(receiver_->GenerateCongestionFeedback(&feedback));
  sender_->OnIncomingQuicCongestionFeedbackFrame(feedback, clock_.Now(),
                                                 not_used_);
  // Make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());

  SendAvailableSendWindow();
  AckNPackets(2);
  // Make sure we fall out of slow start.
  sender_->OnPacketLost(acked_sequence_number_ + 1, clock_.Now());

  for (int i = 0; i < kNumberOfAck; ++i) {
    // Send our full send window.
    SendAvailableSendWindow();
    AckNPackets(2);
  }

  QuicByteCount expected_send_window =
      kMaxCongestionWindowTCP * kDefaultTCPMSS;
  EXPECT_EQ(expected_send_window, sender_->SendWindow());
}

TEST_F(TcpCubicSenderTest, TcpCubicMaxCongestionWindow) {
  const QuicTcpCongestionWindow kMaxCongestionWindowTCP = 50;
  const int kNumberOfAck = 1000;
  sender_.reset(
      new TcpCubicSenderPeer(&clock_, false, kMaxCongestionWindowTCP));

  QuicCongestionFeedbackFrame feedback;
  // At startup make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());
  // Get default QuicCongestionFeedbackFrame from receiver.
  ASSERT_TRUE(receiver_->GenerateCongestionFeedback(&feedback));
  sender_->OnIncomingQuicCongestionFeedbackFrame(feedback, clock_.Now(),
                                                 not_used_);
  // Make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());

  SendAvailableSendWindow();
  AckNPackets(2);
  // Make sure we fall out of slow start.
  sender_->OnPacketLost(acked_sequence_number_ + 1, clock_.Now());

  for (int i = 0; i < kNumberOfAck; ++i) {
    // Send our full send window.
    SendAvailableSendWindow();
    AckNPackets(2);
  }

  QuicByteCount expected_send_window =
      kMaxCongestionWindowTCP * kDefaultTCPMSS;
  EXPECT_EQ(expected_send_window, sender_->SendWindow());
}

TEST_F(TcpCubicSenderTest, MultipleLossesInOneWindow) {
  SendAvailableSendWindow();
  const QuicByteCount initial_window = sender_->GetCongestionWindow();
  sender_->OnPacketLost(acked_sequence_number_ + 1, clock_.Now());
  const QuicByteCount post_loss_window = sender_->GetCongestionWindow();
  EXPECT_GT(initial_window, post_loss_window);
  sender_->OnPacketLost(acked_sequence_number_ + 3, clock_.Now());
  EXPECT_EQ(post_loss_window, sender_->GetCongestionWindow());
  sender_->OnPacketLost(sequence_number_ - 1, clock_.Now());
  EXPECT_EQ(post_loss_window, sender_->GetCongestionWindow());

  // Lose a later packet and ensure the window decreases.
  sender_->OnPacketLost(sequence_number_, clock_.Now());
  EXPECT_GT(post_loss_window, sender_->GetCongestionWindow());
}

TEST_F(TcpCubicSenderTest, SendWindowNotAffectedByAcks) {
  QuicByteCount send_window = sender_->AvailableSendWindow();

  // Send a packet with no retransmittable data, and ensure that the congestion
  // window doesn't change.
  QuicByteCount bytes_in_packet = std::min(kDefaultTCPMSS, send_window);
  sender_->OnPacketSent(clock_.Now(), sequence_number_++, bytes_in_packet,
                        NOT_RETRANSMISSION, NO_RETRANSMITTABLE_DATA);
  EXPECT_EQ(send_window, sender_->AvailableSendWindow());

  // Send a data packet with retransmittable data, and ensure that the
  // congestion window has shrunk.
  sender_->OnPacketSent(clock_.Now(), sequence_number_++, bytes_in_packet,
                        NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
  EXPECT_GT(send_window, sender_->AvailableSendWindow());
}

TEST_F(TcpCubicSenderTest, ConfigureMaxInitialWindow) {
  QuicTcpCongestionWindow congestion_window = sender_->congestion_window();
  QuicConfig config;
  config.set_server_initial_congestion_window(2 * congestion_window,
                                       2 * congestion_window);
  EXPECT_EQ(2 * congestion_window, config.server_initial_congestion_window());

  sender_->SetFromConfig(config, true);
  EXPECT_EQ(2 * congestion_window, sender_->congestion_window());
}

}  // namespace test
}  // namespace net
