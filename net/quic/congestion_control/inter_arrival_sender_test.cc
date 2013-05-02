// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "net/quic/congestion_control/inter_arrival_sender.h"
#include "net/quic/test_tools/mock_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

class InterArrivalSenderTest : public ::testing::Test {
 protected:
  InterArrivalSenderTest()
     : rtt_(QuicTime::Delta::FromMilliseconds(60)),
       one_ms_(QuicTime::Delta::FromMilliseconds(1)),
       one_s_(QuicTime::Delta::FromMilliseconds(1000)),
       nine_ms_(QuicTime::Delta::FromMilliseconds(9)),
       send_start_time_(send_clock_.Now()),
       sender_(&send_clock_),
       sequence_number_(1),
       acked_sequence_number_(1),
       feedback_sequence_number_(1) {
    send_clock_.AdvanceTime(one_ms_);
    receive_clock_.AdvanceTime(one_ms_);
  }

  virtual ~InterArrivalSenderTest() {
    STLDeleteValues(&sent_packets_);
  }

  void SendAvailableCongestionWindow() {
    while (sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                 HAS_RETRANSMITTABLE_DATA).IsZero()) {
      QuicByteCount bytes_in_packet = kMaxPacketSize;
      sent_packets_[sequence_number_] =
          new class SendAlgorithmInterface::SentPacket(
              bytes_in_packet, send_clock_.Now());

      sender_.SentPacket(send_clock_.Now(), sequence_number_, bytes_in_packet,
                         NOT_RETRANSMISSION);
      sequence_number_++;
    }
    EXPECT_FALSE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                       HAS_RETRANSMITTABLE_DATA).IsZero());
  }

  void AckNPackets(int n) {
    for (int i = 0; i < n; ++i) {
      sender_.OnIncomingAck(acked_sequence_number_++, kMaxPacketSize, rtt_);
    }
  }

  void SendDelaySpikeFeedbackMessage(QuicTime::Delta spike_time) {
    QuicCongestionFeedbackFrame feedback;
    feedback.type = kInterArrival;
    feedback.inter_arrival.accumulated_number_of_lost_packets = 0;
    receive_clock_.AdvanceTime(spike_time);
    QuicTime receive_time = receive_clock_.ApproximateNow();
    feedback.inter_arrival.received_packet_times.insert(
        std::pair<QuicPacketSequenceNumber, QuicTime>(
            feedback_sequence_number_, receive_time));
    feedback_sequence_number_++;

    // We need to send feedback for 2 packets since they where sent at the
    // same time.
    feedback.inter_arrival.received_packet_times.insert(
        std::pair<QuicPacketSequenceNumber, QuicTime>(
            feedback_sequence_number_, receive_time));
    feedback_sequence_number_++;

    sender_.OnIncomingQuicCongestionFeedbackFrame(
        feedback, send_clock_.Now(), QuicBandwidth::Zero(), sent_packets_);
  }

  void SendFeedbackMessageNPackets(int n,
                                   QuicTime::Delta delta_odd,
                                   QuicTime::Delta delta_even) {
    QuicCongestionFeedbackFrame feedback;
    feedback.type = kInterArrival;
    feedback.inter_arrival.accumulated_number_of_lost_packets = 0;
    for (int i = 0; i < n; ++i) {
      if (feedback_sequence_number_ % 2) {
        receive_clock_.AdvanceTime(delta_even);
      } else {
        receive_clock_.AdvanceTime(delta_odd);
      }
      QuicTime receive_time = receive_clock_.ApproximateNow();
      feedback.inter_arrival.received_packet_times.insert(
          std::pair<QuicPacketSequenceNumber, QuicTime>(
              feedback_sequence_number_, receive_time));
      feedback_sequence_number_++;
    }
    sender_.OnIncomingQuicCongestionFeedbackFrame(feedback, send_clock_.Now(),
        QuicBandwidth::Zero(), sent_packets_);
  }

  QuicTime::Delta SenderDeltaSinceStart() {
    return send_clock_.ApproximateNow().Subtract(send_start_time_);
  }

  const QuicTime::Delta rtt_;
  const QuicTime::Delta one_ms_;
  const QuicTime::Delta one_s_;
  const QuicTime::Delta nine_ms_;
  MockClock send_clock_;
  MockClock receive_clock_;
  const QuicTime send_start_time_;
  InterArrivalSender sender_;
  QuicPacketSequenceNumber sequence_number_;
  QuicPacketSequenceNumber acked_sequence_number_;
  QuicPacketSequenceNumber feedback_sequence_number_;
  SendAlgorithmInterface::SentPacketsMap sent_packets_;
};

TEST_F(InterArrivalSenderTest, ProbeFollowedByFullRampUpCycle) {
  QuicCongestionFeedbackFrame feedback;
  // At startup make sure we can send.
  EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                    HAS_RETRANSMITTABLE_DATA).IsZero());

  // Send 5 bursts.
  for (int i = 0; i < 4; ++i) {
    SendAvailableCongestionWindow();
    send_clock_.AdvanceTime(sender_.TimeUntilSend(
        send_clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA));
    EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                      HAS_RETRANSMITTABLE_DATA).IsZero());
  }
  SendAvailableCongestionWindow();

  // We have now sent our probe.
  EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                    HAS_RETRANSMITTABLE_DATA).IsInfinite());

  AckNPackets(10);
  SendFeedbackMessageNPackets(10, one_ms_, nine_ms_);
  send_clock_.AdvanceTime(sender_.TimeUntilSend(
      send_clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA));
  EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                    HAS_RETRANSMITTABLE_DATA).IsZero());

  // We should now have our probe rate.
  QuicTime::Delta acc_arrival_time = QuicTime::Delta::FromMilliseconds(41);
  int64 probe_rate = kMaxPacketSize * 9 * kNumMicrosPerSecond /
      acc_arrival_time.ToMicroseconds();
  EXPECT_NEAR(0.7f * probe_rate,
              sender_.BandwidthEstimate().ToBytesPerSecond(), 1000);
  DLOG(INFO) << "After probe";
  // Send 50 bursts, make sure that we move fast in the beginning.
  for (int i = 0; i < 50; ++i) {
    SendAvailableCongestionWindow();
    QuicTime::Delta time_until_send = sender_.TimeUntilSend(
        send_clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
    send_clock_.AdvanceTime(time_until_send);
    EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                      HAS_RETRANSMITTABLE_DATA).IsZero());
    AckNPackets(2);
    SendFeedbackMessageNPackets(2, one_ms_, time_until_send.Subtract(one_ms_));
  }
  EXPECT_NEAR(0.875f * probe_rate,
              sender_.BandwidthEstimate().ToBytesPerSecond(), 1000);
  EXPECT_NEAR(SenderDeltaSinceStart().ToMilliseconds(), 600, 10);

  // Send 50 bursts, make sure that we slow down towards the probe rate.
  for (int i = 0; i < 50; ++i) {
    SendAvailableCongestionWindow();
    QuicTime::Delta time_until_send = sender_.TimeUntilSend(
        send_clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
    send_clock_.AdvanceTime(time_until_send);
    EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                      HAS_RETRANSMITTABLE_DATA).IsZero());
    AckNPackets(2);
    SendFeedbackMessageNPackets(2, one_ms_, time_until_send.Subtract(one_ms_));
  }
  EXPECT_NEAR(0.95f * probe_rate,
              sender_.BandwidthEstimate().ToBytesPerSecond(), 1000);
  EXPECT_NEAR(SenderDeltaSinceStart().ToMilliseconds(), 1100, 10);

  // Send 50 bursts, make sure that we move very slow close to the probe rate.
  for (int i = 0; i < 50; ++i) {
    SendAvailableCongestionWindow();
    QuicTime::Delta time_until_send = sender_.TimeUntilSend(
        send_clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
    send_clock_.AdvanceTime(time_until_send);
    EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                      HAS_RETRANSMITTABLE_DATA).IsZero());
    AckNPackets(2);
    SendFeedbackMessageNPackets(2, one_ms_, time_until_send.Subtract(one_ms_));
  }
  EXPECT_NEAR(0.99f * probe_rate,
              sender_.BandwidthEstimate().ToBytesPerSecond(), 1000);
  EXPECT_NEAR(SenderDeltaSinceStart().ToMilliseconds(), 1560, 10);
  DLOG(INFO) << "Near available channel estimate";

  // Send 50 bursts, make sure that we move very slow close to the probe rate.
  for (int i = 0; i < 50; ++i) {
    SendAvailableCongestionWindow();
    QuicTime::Delta time_until_send = sender_.TimeUntilSend(
        send_clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
    send_clock_.AdvanceTime(time_until_send);
    EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                      HAS_RETRANSMITTABLE_DATA).IsZero());
    AckNPackets(2);
    SendFeedbackMessageNPackets(2, one_ms_, time_until_send.Subtract(one_ms_));
  }
  EXPECT_NEAR(1.00f * probe_rate,
              sender_.BandwidthEstimate().ToBytesPerSecond(), 2000);
  EXPECT_NEAR(SenderDeltaSinceStart().ToMilliseconds(), 2000, 100);
  DLOG(INFO) << "At available channel estimate";

  // Send 50 bursts, make sure that we move very slow close to the probe rate.
  for (int i = 0; i < 50; ++i) {
    SendAvailableCongestionWindow();
    QuicTime::Delta time_until_send = sender_.TimeUntilSend(
        send_clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
    send_clock_.AdvanceTime(time_until_send);
    EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                      HAS_RETRANSMITTABLE_DATA).IsZero());
    AckNPackets(2);
    SendFeedbackMessageNPackets(2, one_ms_, time_until_send.Subtract(one_ms_));
  }
  EXPECT_NEAR(1.01f * probe_rate,
              sender_.BandwidthEstimate().ToBytesPerSecond(), 2000);
  EXPECT_NEAR(SenderDeltaSinceStart().ToMilliseconds(), 2500, 100);

  // Send 50 bursts, make sure that we accelerate after the probe rate.
  for (int i = 0; i < 50; ++i) {
    SendAvailableCongestionWindow();
    QuicTime::Delta time_until_send = sender_.TimeUntilSend(
        send_clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
    send_clock_.AdvanceTime(time_until_send);
    EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                      HAS_RETRANSMITTABLE_DATA).IsZero());
    AckNPackets(2);
    SendFeedbackMessageNPackets(2, one_ms_, time_until_send.Subtract(one_ms_));
  }
  EXPECT_NEAR(1.01f * probe_rate,
              sender_.BandwidthEstimate().ToBytesPerSecond(), 2000);
  EXPECT_NEAR(SenderDeltaSinceStart().ToMilliseconds(), 2900, 100);

  // Send 50 bursts, make sure that we accelerate after the probe rate.
  for (int i = 0; i < 50; ++i) {
    SendAvailableCongestionWindow();
    QuicTime::Delta time_until_send = sender_.TimeUntilSend(
        send_clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
    send_clock_.AdvanceTime(time_until_send);
    EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                      HAS_RETRANSMITTABLE_DATA).IsZero());
    AckNPackets(2);
    SendFeedbackMessageNPackets(2, one_ms_, time_until_send.Subtract(one_ms_));
  }
  EXPECT_NEAR(1.03f * probe_rate,
              sender_.BandwidthEstimate().ToBytesPerSecond(), 2000);
  EXPECT_NEAR(SenderDeltaSinceStart().ToMilliseconds(), 3400, 100);

  int64 max_rate = kMaxPacketSize * kNumMicrosPerSecond /
      one_ms_.ToMicroseconds();

  int64 halfway_rate = probe_rate + (max_rate - probe_rate) / 2;

  // Send until we reach halfway point.
  for (int i = 0; i < 570; ++i) {
    SendAvailableCongestionWindow();
    QuicTime::Delta time_until_send = sender_.TimeUntilSend(
        send_clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
    send_clock_.AdvanceTime(time_until_send);
    EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                      HAS_RETRANSMITTABLE_DATA).IsZero());
    AckNPackets(2);
    SendFeedbackMessageNPackets(2, one_ms_, time_until_send.Subtract(one_ms_));
  }
  EXPECT_NEAR(halfway_rate,
              sender_.BandwidthEstimate().ToBytesPerSecond(), 5000);
  EXPECT_NEAR(SenderDeltaSinceStart().ToMilliseconds(), 6600, 100);
  DLOG(INFO) << "Near halfway point";

  // Send until we reach max channel capacity.
  for (int i = 0; i < 1500; ++i) {
    SendAvailableCongestionWindow();
    QuicTime::Delta time_until_send = sender_.TimeUntilSend(
        send_clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
    send_clock_.AdvanceTime(time_until_send);
    EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                      HAS_RETRANSMITTABLE_DATA).IsZero());
    AckNPackets(2);
    SendFeedbackMessageNPackets(2, one_ms_, time_until_send.Subtract(one_ms_));
  }
  EXPECT_NEAR(max_rate,
              sender_.BandwidthEstimate().ToBytesPerSecond(), 5000);
  EXPECT_NEAR(SenderDeltaSinceStart().ToMilliseconds(), 10000, 200);
}

TEST_F(InterArrivalSenderTest, DelaySpikeFollowedBySlowDrain) {
  QuicCongestionFeedbackFrame feedback;
  // At startup make sure we can send.
  EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                    HAS_RETRANSMITTABLE_DATA).IsZero());

  // Send 5 bursts.
  for (int i = 0; i < 4; ++i) {
    SendAvailableCongestionWindow();
    send_clock_.AdvanceTime(sender_.TimeUntilSend(
        send_clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA));
    EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                      HAS_RETRANSMITTABLE_DATA).IsZero());
  }
  SendAvailableCongestionWindow();

  // We have now sent our probe.
  EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                    HAS_RETRANSMITTABLE_DATA).IsInfinite());

  AckNPackets(10);
  SendFeedbackMessageNPackets(10, one_ms_, nine_ms_);
  send_clock_.AdvanceTime(sender_.TimeUntilSend(
      send_clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA));
  EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                    HAS_RETRANSMITTABLE_DATA).IsZero());

  // We should now have our probe rate.
  QuicTime::Delta acc_arrival_time = QuicTime::Delta::FromMilliseconds(41);
  int64 probe_rate = kMaxPacketSize * 9 * kNumMicrosPerSecond /
      acc_arrival_time.ToMicroseconds();
  EXPECT_NEAR(0.7f * probe_rate,
              sender_.BandwidthEstimate().ToBytesPerSecond(), 1000);

  // Send 50 bursts, make sure that we move fast in the beginning.
  for (int i = 0; i < 50; ++i) {
    SendAvailableCongestionWindow();
    QuicTime::Delta time_until_send = sender_.TimeUntilSend(
        send_clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
    send_clock_.AdvanceTime(time_until_send);
    EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                      HAS_RETRANSMITTABLE_DATA).IsZero());
    AckNPackets(2);
    SendFeedbackMessageNPackets(2, one_ms_, time_until_send.Subtract(one_ms_));
  }
  EXPECT_NEAR(0.875f * probe_rate,
              sender_.BandwidthEstimate().ToBytesPerSecond(), 1000);
  EXPECT_NEAR(SenderDeltaSinceStart().ToMilliseconds(), 600, 10);

  SendAvailableCongestionWindow();
  send_clock_.AdvanceTime(sender_.TimeUntilSend(
      send_clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA));
  EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                    HAS_RETRANSMITTABLE_DATA).IsZero());
  AckNPackets(2);

  int64 rate_at_introduced_delay_spike = 0.875f * probe_rate;
  QuicTime::Delta spike_time = QuicTime::Delta::FromMilliseconds(100);
  SendDelaySpikeFeedbackMessage(spike_time);

  // Backing as much as we can, currently 90%.
  EXPECT_NEAR(0.1f * rate_at_introduced_delay_spike,
              sender_.BandwidthEstimate().ToBytesPerSecond(), 1000);
  EXPECT_NEAR(SenderDeltaSinceStart().ToMilliseconds(), 610, 10);

  // Run until we are catched up after our introduced delay spike.
  while (send_clock_.Now() < receive_clock_.Now()) {
    SendAvailableCongestionWindow();
    send_clock_.AdvanceTime(sender_.TimeUntilSend(
        send_clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA));
    EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                      HAS_RETRANSMITTABLE_DATA).IsZero());
    AckNPackets(2);
    SendFeedbackMessageNPackets(2, one_ms_, one_ms_);
  }
  // Expect that we go back to 67% of the rate before the spike.
  EXPECT_NEAR(0.67f * rate_at_introduced_delay_spike,
              sender_.BandwidthEstimate().ToBytesPerSecond(), 1000);
  EXPECT_NEAR(SenderDeltaSinceStart().ToMilliseconds(), 820, 10);

  // Send 100 bursts, make sure that we slow down towards the rate we had
  // before the spike.
  for (int i = 0; i < 100; ++i) {
    SendAvailableCongestionWindow();
    QuicTime::Delta time_until_send = sender_.TimeUntilSend(
        send_clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
    send_clock_.AdvanceTime(time_until_send);
    EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                      HAS_RETRANSMITTABLE_DATA).IsZero());
    AckNPackets(2);
    SendFeedbackMessageNPackets(2, one_ms_, time_until_send.Subtract(one_ms_));
  }
  EXPECT_NEAR(0.97f * rate_at_introduced_delay_spike,
              sender_.BandwidthEstimate().ToBytesPerSecond(), 1000);
  EXPECT_NEAR(SenderDeltaSinceStart().ToMilliseconds(), 2100, 10);
}

TEST_F(InterArrivalSenderTest, DelaySpikeFollowedByImmediateDrain) {
  QuicCongestionFeedbackFrame feedback;
  // At startup make sure we can send.
  EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                    HAS_RETRANSMITTABLE_DATA).IsZero());

  // Send 5 bursts.
  for (int i = 0; i < 4; ++i) {
    SendAvailableCongestionWindow();
    send_clock_.AdvanceTime(sender_.TimeUntilSend(
        send_clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA));
    EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                      HAS_RETRANSMITTABLE_DATA).IsZero());
  }
  SendAvailableCongestionWindow();

  // We have now sent our probe.
  EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                    HAS_RETRANSMITTABLE_DATA).IsInfinite());

  AckNPackets(10);
  SendFeedbackMessageNPackets(10, one_ms_, nine_ms_);
  send_clock_.AdvanceTime(sender_.TimeUntilSend(
      send_clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA));
  EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                    HAS_RETRANSMITTABLE_DATA).IsZero());

  // We should now have our probe rate.
  QuicTime::Delta acc_arrival_time = QuicTime::Delta::FromMilliseconds(41);
  int64 probe_rate = kMaxPacketSize * 9 * kNumMicrosPerSecond /
      acc_arrival_time.ToMicroseconds();
  EXPECT_NEAR(0.7f * probe_rate,
              sender_.BandwidthEstimate().ToBytesPerSecond(), 1000);

  // Send 50 bursts, make sure that we move fast in the beginning.
  for (int i = 0; i < 50; ++i) {
    SendAvailableCongestionWindow();
    QuicTime::Delta time_until_send = sender_.TimeUntilSend(
        send_clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
    send_clock_.AdvanceTime(time_until_send);
    EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                      HAS_RETRANSMITTABLE_DATA).IsZero());
    AckNPackets(2);
    SendFeedbackMessageNPackets(2, one_ms_, time_until_send.Subtract(one_ms_));
  }
  EXPECT_NEAR(0.875f * probe_rate,
              sender_.BandwidthEstimate().ToBytesPerSecond(), 1000);
  EXPECT_NEAR(SenderDeltaSinceStart().ToMilliseconds(), 600, 10);

  SendAvailableCongestionWindow();
  send_clock_.AdvanceTime(sender_.TimeUntilSend(
      send_clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA));
  EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                    HAS_RETRANSMITTABLE_DATA).IsZero());
  AckNPackets(2);

  int64 rate_at_introduced_delay_spike = 0.875f * probe_rate;
  QuicTime::Delta spike_time = QuicTime::Delta::FromMilliseconds(100);
  SendDelaySpikeFeedbackMessage(spike_time);

  // Backing as much as we can, currently 90%.
  EXPECT_NEAR(0.1f * rate_at_introduced_delay_spike,
              sender_.BandwidthEstimate().ToBytesPerSecond(), 1000);
  EXPECT_NEAR(SenderDeltaSinceStart().ToMilliseconds(), 610, 10);

  // Move send time forward.
  send_clock_.AdvanceTime(spike_time);
  // Make sure our clocks are aligned again.
  receive_clock_.AdvanceTime(send_clock_.Now().Subtract(receive_clock_.Now()));

  SendAvailableCongestionWindow();
  AckNPackets(2);
  SendFeedbackMessageNPackets(2, one_ms_, one_ms_);
  // We should now be back where we introduced the delay spike.
  EXPECT_NEAR(rate_at_introduced_delay_spike,
              sender_.BandwidthEstimate().ToBytesPerSecond(), 1000);
  EXPECT_NEAR(SenderDeltaSinceStart().ToMilliseconds(), 710, 10);
}

TEST_F(InterArrivalSenderTest, MinBitrateDueToDelay) {
  QuicBandwidth expected_min_bitrate = QuicBandwidth::FromKBitsPerSecond(10);
  QuicCongestionFeedbackFrame feedback;
  // At startup make sure we can send.
  EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                    HAS_RETRANSMITTABLE_DATA).IsZero());

  // Send 5 bursts.
  for (int i = 0; i < 4; ++i) {
    SendAvailableCongestionWindow();
    send_clock_.AdvanceTime(sender_.TimeUntilSend(
        send_clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA));
    EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                      HAS_RETRANSMITTABLE_DATA).IsZero());
  }
  SendAvailableCongestionWindow();

  AckNPackets(10);

  // One second spread per packet is expected to result in an estimate at
  // our minimum bitrate.
  SendFeedbackMessageNPackets(10, one_s_, one_s_);
  send_clock_.AdvanceTime(sender_.TimeUntilSend(
      send_clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA));
  EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                    HAS_RETRANSMITTABLE_DATA).IsZero());
  EXPECT_EQ(expected_min_bitrate, sender_.BandwidthEstimate());
}

TEST_F(InterArrivalSenderTest, MinBitrateDueToLoss) {
  QuicBandwidth expected_min_bitrate = QuicBandwidth::FromKBitsPerSecond(10);
  QuicCongestionFeedbackFrame feedback;
  // At startup make sure we can send.
  EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                    HAS_RETRANSMITTABLE_DATA).IsZero());

  // Send 5 bursts.
  for (int i = 0; i < 4; ++i) {
    SendAvailableCongestionWindow();
    send_clock_.AdvanceTime(sender_.TimeUntilSend(
        send_clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA));
    EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                      HAS_RETRANSMITTABLE_DATA).IsZero());
  }
  SendAvailableCongestionWindow();

  AckNPackets(10);
  SendFeedbackMessageNPackets(10, nine_ms_, nine_ms_);
  send_clock_.AdvanceTime(sender_.TimeUntilSend(
      send_clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA));
  EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                    HAS_RETRANSMITTABLE_DATA).IsZero());

  QuicTime::Delta acc_arrival_time = QuicTime::Delta::FromMilliseconds(81);
  int64 probe_rate = kMaxPacketSize * 9 * kNumMicrosPerSecond /
      acc_arrival_time.ToMicroseconds();
  EXPECT_NEAR(0.7f * probe_rate,
              sender_.BandwidthEstimate().ToBytesPerSecond(), 1000);

  for (int i = 0; i < 15; ++i) {
    SendAvailableCongestionWindow();
    QuicTime::Delta time_until_send = sender_.TimeUntilSend(
        send_clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
    send_clock_.AdvanceTime(time_until_send);
    EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                      HAS_RETRANSMITTABLE_DATA).IsZero());
    sender_.OnIncomingLoss(send_clock_.Now());
    sender_.OnIncomingAck(acked_sequence_number_, kMaxPacketSize, rtt_);
    acked_sequence_number_ += 2;  // Create a loss by not acking both packets.
    SendFeedbackMessageNPackets(2, nine_ms_, nine_ms_);
  }
  // Test that our exponentail back off stop at expected_min_bitrate.
  EXPECT_EQ(expected_min_bitrate, sender_.BandwidthEstimate());

  for (int i = 0; i < 50; ++i) {
    SendAvailableCongestionWindow();
    QuicTime::Delta time_until_send = sender_.TimeUntilSend(
        send_clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
    send_clock_.AdvanceTime(time_until_send);
    EXPECT_TRUE(sender_.TimeUntilSend(send_clock_.Now(), NOT_RETRANSMISSION,
                                      HAS_RETRANSMITTABLE_DATA).IsZero());
    sender_.OnIncomingLoss(send_clock_.Now());
    sender_.OnIncomingAck(acked_sequence_number_, kMaxPacketSize, rtt_);
    acked_sequence_number_ += 2;  // Create a loss by not acking both packets.
    SendFeedbackMessageNPackets(2, nine_ms_, nine_ms_);

    // Make sure our bitrate is fixed at the expected_min_bitrate.
    EXPECT_EQ(expected_min_bitrate, sender_.BandwidthEstimate());
  }
}

}  // namespace test
}  // namespace net
