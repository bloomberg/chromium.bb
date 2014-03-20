// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/congestion_control/rtt_stats.h"
#include "net/quic/congestion_control/tcp_cubic_sender.h"
#include "net/quic/congestion_control/tcp_receiver.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/mock_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::min;

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
      : TcpCubicSender(
            clock, &rtt_stats_, reno, max_tcp_congestion_window, &stats_) {
  }

  QuicTcpCongestionWindow congestion_window() {
    return congestion_window_;
  }

  RttStats rtt_stats_;
  QuicConnectionStats stats_;

  using TcpCubicSender::AvailableSendWindow;
  using TcpCubicSender::SendWindow;
};

class TcpCubicSenderTest : public ::testing::Test {
 protected:
  TcpCubicSenderTest()
      : one_ms_(QuicTime::Delta::FromMilliseconds(1)),
        sender_(new TcpCubicSenderPeer(&clock_, true,
                                       kDefaultMaxCongestionWindowTCP)),
        receiver_(new TcpReceiver()),
        sequence_number_(1),
        acked_sequence_number_(0) {
  }

  int SendAvailableSendWindow() {
    // Send as long as TimeUntilSend returns Zero.
    int packets_sent = 0;
    bool can_send = sender_->TimeUntilSend(
        clock_.Now(), NOT_RETRANSMISSION,
        HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero();
    while (can_send) {
      sender_->OnPacketSent(clock_.Now(), sequence_number_++, kDefaultTCPMSS,
                            NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
      ++packets_sent;
      can_send = sender_->TimeUntilSend(
          clock_.Now(), NOT_RETRANSMISSION,
          HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero();
    }
    return packets_sent;
  }

  void UpdateRtt(QuicTime::Delta rtt) {
    sender_->rtt_stats_.UpdateRtt(rtt, QuicTime::Delta::Zero());
    sender_->UpdateRtt(rtt);
  }

  // Normal is that TCP acks every other segment.
  void AckNPackets(int n) {
    for (int i = 0; i < n; ++i) {
      ++acked_sequence_number_;
      UpdateRtt(QuicTime::Delta::FromMilliseconds(60));
      sender_->OnPacketAcked(acked_sequence_number_, kDefaultTCPMSS);
    }
    clock_.AdvanceTime(one_ms_);  // 1 millisecond.
  }

  void LoseNPackets(int n) {
    for (int i = 0; i < n; ++i) {
      ++acked_sequence_number_;
      sender_->OnPacketAbandoned(acked_sequence_number_, kDefaultTCPMSS);
      sender_->OnPacketLost(acked_sequence_number_, clock_.Now());
    }
  }

  const QuicTime::Delta one_ms_;
  MockClock clock_;
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
  sender_->OnIncomingQuicCongestionFeedbackFrame(feedback, clock_.Now());
  // Make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());
  // And that window is un-affected.
  EXPECT_EQ(kDefaultWindowTCP, sender_->AvailableSendWindow());
  EXPECT_EQ(kDefaultWindowTCP, sender_->GetCongestionWindow());

  // A retransmit should always return 0.
  for (int i = FIRST_TRANSMISSION_TYPE; i <= LAST_TRANSMISSION_TYPE; ++i) {
    TransmissionType type = static_cast<TransmissionType>(i);
    EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
                                       type,
                                       HAS_RETRANSMITTABLE_DATA,
                                       NOT_HANDSHAKE).IsZero())
        << QuicUtils::TransmissionTypeToString(type);
  }

  // Fill the send window with data, then verify that we can still
  // send handshake and TLP packets.
  SendAvailableSendWindow();
  for (int i = FIRST_TRANSMISSION_TYPE; i <= LAST_TRANSMISSION_TYPE; ++i) {
    TransmissionType type = static_cast<TransmissionType>(i);
    bool expect_can_send = (type == HANDSHAKE_RETRANSMISSION ||
                            type == TLP_RETRANSMISSION);
    EXPECT_EQ(expect_can_send,
              sender_->TimeUntilSend(clock_.Now(),
                                     type,
                                     HAS_RETRANSMITTABLE_DATA,
                                     NOT_HANDSHAKE).IsZero())
        << QuicUtils::TransmissionTypeToString(type);
  }
}

TEST_F(TcpCubicSenderTest, ExponentialSlowStart) {
  const int kNumberOfAcks = 20;
  QuicCongestionFeedbackFrame feedback;
  // At startup make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());
  // Get default QuicCongestionFeedbackFrame from receiver.
  ASSERT_TRUE(receiver_->GenerateCongestionFeedback(&feedback));
  sender_->OnIncomingQuicCongestionFeedbackFrame(feedback, clock_.Now());
  // Make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());

  for (int i = 0; i < kNumberOfAcks; ++i) {
    // Send our full send window.
    SendAvailableSendWindow();
    AckNPackets(2);
  }
  QuicByteCount bytes_to_send = sender_->SendWindow();
  EXPECT_EQ(kDefaultWindowTCP + kDefaultTCPMSS * 2 * kNumberOfAcks,
            bytes_to_send);
}

TEST_F(TcpCubicSenderTest, SlowStartAckTrain) {
  // Make sure that we fall out of slow start when we send ACK train longer
  // than half the RTT, in this test case 30ms, which is more than 30 calls to
  // Ack2Packets in one round.
  // Since we start at 10 packet first round will be 5 second round 10 etc
  // Hence we should pass 30 at 65 = 5 + 10 + 20 + 30
  const int kNumberOfAcks = 65;
  QuicCongestionFeedbackFrame feedback;
  // At startup make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());
  // Get default QuicCongestionFeedbackFrame from receiver.
  ASSERT_TRUE(receiver_->GenerateCongestionFeedback(&feedback));
  sender_->OnIncomingQuicCongestionFeedbackFrame(feedback, clock_.Now());
  // Make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());

  for (int i = 0; i < kNumberOfAcks; ++i) {
    // Send our full send window.
    SendAvailableSendWindow();
    AckNPackets(2);
  }
  QuicByteCount expected_send_window =
      kDefaultWindowTCP + (kDefaultTCPMSS * 2 * kNumberOfAcks);
  EXPECT_EQ(expected_send_window, sender_->GetCongestionWindow());
  EXPECT_EQ(expected_send_window, sender_->GetCongestionWindow());

  // We should now have fallen out of slow start.
  // Testing Reno phase.
  // We should need 140(65*2+10) ACK:ed packets before increasing window by
  // one.
  for (int i = 0; i < 69; ++i) {
    SendAvailableSendWindow();
    AckNPackets(2);
    EXPECT_EQ(expected_send_window, sender_->GetCongestionWindow());
  }
  SendAvailableSendWindow();
  AckNPackets(2);
  expected_send_window += kDefaultTCPMSS;
  EXPECT_EQ(expected_send_window, sender_->GetCongestionWindow());
}

TEST_F(TcpCubicSenderTest, SlowStartPacketLoss) {
  // Make sure that we fall out of slow start when we encounter a packet loss.
  QuicCongestionFeedbackFrame feedback;
  // At startup make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());
  // Get default QuicCongestionFeedbackFrame from receiver.
  ASSERT_TRUE(receiver_->GenerateCongestionFeedback(&feedback));
  sender_->OnIncomingQuicCongestionFeedbackFrame(feedback, clock_.Now());
  // Make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());

  const int kNumberOfAcks = 10;
  for (int i = 0; i < kNumberOfAcks; ++i) {
    // Send our full send window.
    SendAvailableSendWindow();
    AckNPackets(2);
  }
  SendAvailableSendWindow();
  QuicByteCount expected_send_window = kDefaultWindowTCP +
      (kDefaultTCPMSS * 2 * kNumberOfAcks);
  EXPECT_EQ(expected_send_window, sender_->GetCongestionWindow());

  sender_->OnPacketLost(acked_sequence_number_ + 1, clock_.Now());
  ++acked_sequence_number_;

  // Make sure that we can send right now due to limited transmit.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(), NOT_RETRANSMISSION,
      HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());

  // We should now have fallen out of slow start.
  // We expect window to be cut in half by Reno.
  expected_send_window /= 2;
  EXPECT_EQ(expected_send_window, sender_->GetCongestionWindow());

  // Testing Reno phase.
  // We need to ack half of the pending packet before we can send again.
  size_t number_of_packets_in_window = expected_send_window / kDefaultTCPMSS;
  AckNPackets(number_of_packets_in_window);
  EXPECT_EQ(expected_send_window, sender_->GetCongestionWindow());
  EXPECT_EQ(0u, sender_->AvailableSendWindow());

  // We need to ack every packet in the window before we exit recovery.
  for (size_t i = 0; i < number_of_packets_in_window; ++i) {
    AckNPackets(1);
    SendAvailableSendWindow();
    EXPECT_EQ(expected_send_window, sender_->GetCongestionWindow());
  }

  // We need to ack another window before we increase CWND by 1.
  for (size_t i = 0; i < number_of_packets_in_window - 2; ++i) {
    AckNPackets(1);
    SendAvailableSendWindow();
    EXPECT_EQ(expected_send_window, sender_->GetCongestionWindow());
  }

  AckNPackets(1);
  expected_send_window += kDefaultTCPMSS;
  EXPECT_EQ(expected_send_window, sender_->GetCongestionWindow());
}

TEST_F(TcpCubicSenderTest, SlowStartPacketLossPRR) {
  // Test based on the first example in RFC6937.
  // Make sure that we fall out of slow start when we encounter a packet loss.
  QuicCongestionFeedbackFrame feedback;
  // At startup make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());
  // Get default QuicCongestionFeedbackFrame from receiver.
  ASSERT_TRUE(receiver_->GenerateCongestionFeedback(&feedback));
  sender_->OnIncomingQuicCongestionFeedbackFrame(feedback, clock_.Now());

  // Ack 10 packets in 5 acks to raise the CWND to 20, as in the example.
  const int kNumberOfAcks = 5;
  for (int i = 0; i < kNumberOfAcks; ++i) {
    // Send our full send window.
    SendAvailableSendWindow();
    AckNPackets(2);
  }
  SendAvailableSendWindow();
  QuicByteCount expected_send_window = kDefaultWindowTCP +
      (kDefaultTCPMSS * 2 * kNumberOfAcks);
  EXPECT_EQ(expected_send_window, sender_->GetCongestionWindow());

  LoseNPackets(1);

  // We should now have fallen out of slow start.
  // We expect window to be cut in half by Reno.
  expected_send_window /= 2;
  EXPECT_EQ(expected_send_window, sender_->GetCongestionWindow());

  // Send 1 packet to simulate limited transmit.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(), NOT_RETRANSMISSION,
      HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());
  EXPECT_EQ(1, SendAvailableSendWindow());

  // Testing TCP proportional rate reduction.
  // We should send one packet for every two received acks over the remaining
  // 18 outstanding packets.
  size_t number_of_packets_in_window = expected_send_window / kDefaultTCPMSS;
  // The number of packets before we exit recovery is the original CWND minus
  // the packet that has been lost and the one which triggered the loss.
  size_t remaining_packets_in_recovery = number_of_packets_in_window * 2 - 1;
  for (size_t i = 0; i < remaining_packets_in_recovery - 1; i += 2) {
    AckNPackets(2);
    EXPECT_TRUE(sender_->TimeUntilSend(
        clock_.Now(), NOT_RETRANSMISSION,
        HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());
    EXPECT_EQ(0u, sender_->AvailableSendWindow());
    EXPECT_EQ(1, SendAvailableSendWindow());
    EXPECT_EQ(expected_send_window, sender_->GetCongestionWindow());
  }
  // If there is one more packet to ack before completing recovery, ack it.
  if (remaining_packets_in_recovery % 2 == 1) {
    AckNPackets(1);
  }

  // We need to ack another window before we increase CWND by 1.
  for (size_t i = 0; i < number_of_packets_in_window - 1; ++i) {
    AckNPackets(1);
    EXPECT_EQ(1, SendAvailableSendWindow());
    EXPECT_EQ(expected_send_window, sender_->GetCongestionWindow());
  }

  AckNPackets(1);
  expected_send_window += kDefaultTCPMSS;
  EXPECT_EQ(expected_send_window, sender_->GetCongestionWindow());
}

TEST_F(TcpCubicSenderTest, SlowStartBurstPacketLossPRR) {
  // Test based on the second example in RFC6937, though we also implement
  // forward acknowledgements, so the first two incoming acks will trigger
  // PRR immediately.
  // Make sure that we fall out of slow start when we encounter a packet loss.
  QuicCongestionFeedbackFrame feedback;
  // At startup make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());
  // Get default QuicCongestionFeedbackFrame from receiver.
  ASSERT_TRUE(receiver_->GenerateCongestionFeedback(&feedback));
  sender_->OnIncomingQuicCongestionFeedbackFrame(feedback, clock_.Now());

  // Ack 10 packets in 5 acks to raise the CWND to 20, as in the example.
  const int kNumberOfAcks = 5;
  for (int i = 0; i < kNumberOfAcks; ++i) {
    // Send our full send window.
    SendAvailableSendWindow();
    AckNPackets(2);
  }
  SendAvailableSendWindow();
  QuicByteCount expected_send_window = kDefaultWindowTCP +
      (kDefaultTCPMSS * 2 * kNumberOfAcks);
  EXPECT_EQ(expected_send_window, sender_->GetCongestionWindow());

  // Ack a packet with a 15 packet gap, losing 13 of them due to FACK.
  sender_->OnPacketAcked(acked_sequence_number_ + 15, kDefaultTCPMSS);
  LoseNPackets(13);

  // We should now have fallen out of slow start.
  // We expect window to be cut in half by Reno.
  expected_send_window /= 2;
  EXPECT_EQ(expected_send_window, sender_->GetCongestionWindow());

  // Only 2 packets should be allowed to be sent, per PRR-SSRB
  EXPECT_EQ(2, SendAvailableSendWindow());

  // Ack the next packet, which triggers another loss.
  sender_->OnPacketAcked(acked_sequence_number_ + 4, kDefaultTCPMSS);
  LoseNPackets(1);

  // Send 2 packets to simulate PRR-SSRB.
  EXPECT_EQ(2, SendAvailableSendWindow());

  // Ack the next packet, which triggers another loss.
  sender_->OnPacketAcked(acked_sequence_number_ + 4, kDefaultTCPMSS);
  LoseNPackets(1);

  // Send 2 packets to simulate PRR-SSRB.
  EXPECT_EQ(2, SendAvailableSendWindow());

  AckNPackets(1);
  EXPECT_EQ(2, SendAvailableSendWindow());

  AckNPackets(1);
  EXPECT_EQ(2, SendAvailableSendWindow());

  // The window should not have changed.
  EXPECT_EQ(expected_send_window, sender_->GetCongestionWindow());

  // Exit recovery and return to sending at the new rate.
  for (int i = 0; i < kNumberOfAcks; ++i) {
    AckNPackets(1);
    EXPECT_EQ(1, SendAvailableSendWindow());
  }
}

TEST_F(TcpCubicSenderTest, RTOCongestionWindow) {
  EXPECT_EQ(kDefaultWindowTCP, sender_->SendWindow());

  // Expect the window to decrease to the minimum once the RTO fires.
  sender_->OnRetransmissionTimeout(true);
  EXPECT_EQ(2 * kDefaultTCPMSS, sender_->SendWindow());
}

TEST_F(TcpCubicSenderTest, RTOCongestionWindowNoRetransmission) {
  EXPECT_EQ(kDefaultWindowTCP, sender_->SendWindow());

  // Expect the window to remain unchanged if the RTO fires but no
  // packets are retransmitted.
  sender_->OnRetransmissionTimeout(false);
  EXPECT_EQ(kDefaultWindowTCP, sender_->SendWindow());
}

TEST_F(TcpCubicSenderTest, RetransmissionDelay) {
  const int64 kRttMs = 10;
  const int64 kDeviationMs = 3;
  EXPECT_EQ(QuicTime::Delta::Zero(), sender_->RetransmissionDelay());

  UpdateRtt(QuicTime::Delta::FromMilliseconds(kRttMs));

  // Initial value is to set the median deviation to half of the initial
  // rtt, the median in then multiplied by a factor of 4 and finally the
  // smoothed rtt is added which is the initial rtt.
  QuicTime::Delta expected_delay =
      QuicTime::Delta::FromMilliseconds(kRttMs + kRttMs / 2 * 4);
  EXPECT_EQ(expected_delay, sender_->RetransmissionDelay());

  for (int i = 0; i < 100; ++i) {
    // Run to make sure that we converge.
    UpdateRtt(QuicTime::Delta::FromMilliseconds(kRttMs + kDeviationMs));
    UpdateRtt(QuicTime::Delta::FromMilliseconds(kRttMs - kDeviationMs));
  }
  expected_delay = QuicTime::Delta::FromMilliseconds(kRttMs + kDeviationMs * 4);

  EXPECT_NEAR(kRttMs, sender_->rtt_stats_.SmoothedRtt().ToMilliseconds(), 1);
  EXPECT_NEAR(expected_delay.ToMilliseconds(),
              sender_->RetransmissionDelay().ToMilliseconds(),
              1);
  EXPECT_EQ(static_cast<int64>(
                sender_->GetCongestionWindow() * kNumMicrosPerSecond /
                sender_->rtt_stats_.SmoothedRtt().ToMicroseconds()),
            sender_->BandwidthEstimate().ToBytesPerSecond());
}

TEST_F(TcpCubicSenderTest, SlowStartMaxSendWindow) {
  const QuicTcpCongestionWindow kMaxCongestionWindowTCP = 50;
  const int kNumberOfAcks = 100;
  sender_.reset(
      new TcpCubicSenderPeer(&clock_, false, kMaxCongestionWindowTCP));

  QuicCongestionFeedbackFrame feedback;
  // At startup make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());
  // Get default QuicCongestionFeedbackFrame from receiver.
  ASSERT_TRUE(receiver_->GenerateCongestionFeedback(&feedback));
  sender_->OnIncomingQuicCongestionFeedbackFrame(feedback, clock_.Now());
  // Make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());

  for (int i = 0; i < kNumberOfAcks; ++i) {
    // Send our full send window.
    SendAvailableSendWindow();
    AckNPackets(2);
  }
  QuicByteCount expected_send_window =
      kMaxCongestionWindowTCP * kDefaultTCPMSS;
  EXPECT_EQ(expected_send_window, sender_->GetCongestionWindow());
}

TEST_F(TcpCubicSenderTest, TcpRenoMaxCongestionWindow) {
  const QuicTcpCongestionWindow kMaxCongestionWindowTCP = 50;
  const int kNumberOfAcks = 1000;
  sender_.reset(
      new TcpCubicSenderPeer(&clock_, true, kMaxCongestionWindowTCP));

  QuicCongestionFeedbackFrame feedback;
  // At startup make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());
  // Get default QuicCongestionFeedbackFrame from receiver.
  ASSERT_TRUE(receiver_->GenerateCongestionFeedback(&feedback));
  sender_->OnIncomingQuicCongestionFeedbackFrame(feedback, clock_.Now());
  // Make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());

  SendAvailableSendWindow();
  AckNPackets(2);
  // Make sure we fall out of slow start.
  sender_->OnPacketLost(acked_sequence_number_ + 1, clock_.Now());

  for (int i = 0; i < kNumberOfAcks; ++i) {
    // Send our full send window.
    SendAvailableSendWindow();
    AckNPackets(2);
  }

  QuicByteCount expected_send_window =
      kMaxCongestionWindowTCP * kDefaultTCPMSS;
  EXPECT_EQ(expected_send_window, sender_->GetCongestionWindow());
}

TEST_F(TcpCubicSenderTest, TcpCubicMaxCongestionWindow) {
  const QuicTcpCongestionWindow kMaxCongestionWindowTCP = 50;
  // Set to 10000 to compensate for small cubic alpha.
  const int kNumberOfAcks = 10000;

  sender_.reset(
      new TcpCubicSenderPeer(&clock_, false, kMaxCongestionWindowTCP));

  QuicCongestionFeedbackFrame feedback;
  // At startup make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());
  // Get default QuicCongestionFeedbackFrame from receiver.
  ASSERT_TRUE(receiver_->GenerateCongestionFeedback(&feedback));
  sender_->OnIncomingQuicCongestionFeedbackFrame(feedback, clock_.Now());
  // Make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());

  SendAvailableSendWindow();
  AckNPackets(2);
  // Make sure we fall out of slow start.
  sender_->OnPacketLost(acked_sequence_number_ + 1, clock_.Now());

  for (int i = 0; i < kNumberOfAcks; ++i) {
    // Send our full send window.
    SendAvailableSendWindow();
    AckNPackets(2);
  }

  QuicByteCount expected_send_window =
      kMaxCongestionWindowTCP * kDefaultTCPMSS;
  EXPECT_EQ(expected_send_window, sender_->GetCongestionWindow());
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
  QuicByteCount bytes_in_packet = min(kDefaultTCPMSS, send_window);
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

TEST_F(TcpCubicSenderTest, CongestionAvoidanceAtEndOfRecovery) {
  // Make sure that we fall out of slow start when we encounter a packet loss.
  QuicCongestionFeedbackFrame feedback;
  // At startup make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(clock_.Now(),
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA, NOT_HANDSHAKE).IsZero());
  // Get default QuicCongestionFeedbackFrame from receiver.
  ASSERT_TRUE(receiver_->GenerateCongestionFeedback(&feedback));
  sender_->OnIncomingQuicCongestionFeedbackFrame(feedback, clock_.Now());
  // Ack 10 packets in 5 acks to raise the CWND to 20.
  const int kNumberOfAcks = 5;
  for (int i = 0; i < kNumberOfAcks; ++i) {
    // Send our full send window.
    SendAvailableSendWindow();
    AckNPackets(2);
  }
  SendAvailableSendWindow();
  QuicByteCount expected_send_window = kDefaultWindowTCP +
      (kDefaultTCPMSS * 2 * kNumberOfAcks);
  EXPECT_EQ(expected_send_window, sender_->GetCongestionWindow());

  LoseNPackets(1);

  // We should now have fallen out of slow start, and window should be cut in
  // half by Reno. New cwnd should be 10.
  expected_send_window /= 2;
  EXPECT_EQ(expected_send_window, sender_->GetCongestionWindow());

  // No congestion window growth should occur in recovery phase, i.e.,
  // until the currently outstanding 20 packets are acked.
  for (int i = 0; i < 10; ++i) {
    // Send our full send window.
    SendAvailableSendWindow();
    AckNPackets(2);
    EXPECT_EQ(expected_send_window, sender_->GetCongestionWindow());
  }

  // Out of recovery now. Congestion window should not grow during RTT.
  for (int i = 0; i < 4; ++i) {
    // Send our full send window.
    SendAvailableSendWindow();
    AckNPackets(2);
    EXPECT_EQ(expected_send_window, sender_->GetCongestionWindow());
  }

  // Next ack should cause congestion window to grow by 1MSS.
  AckNPackets(2);
  expected_send_window += kDefaultTCPMSS;
  EXPECT_EQ(expected_send_window, sender_->GetCongestionWindow());
}

}  // namespace test
}  // namespace net
