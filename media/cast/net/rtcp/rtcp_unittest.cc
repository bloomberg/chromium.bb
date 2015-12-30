// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/net/pacing/paced_sender.h"
#include "media/cast/net/rtcp/receiver_rtcp_session.h"
#include "media/cast/net/rtcp/rtcp_session.h"
#include "media/cast/net/rtcp/rtcp_utility.h"
#include "media/cast/net/rtcp/sender_rtcp_session.h"
#include "media/cast/test/skewed_tick_clock.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

namespace {

media::cast::RtcpTimeData CreateRtcpTimeData(base::TimeTicks now) {
  media::cast::RtcpTimeData ret;
  ret.timestamp = now;
  media::cast::ConvertTimeTicksToNtp(now, &ret.ntp_seconds, &ret.ntp_fraction);
  return ret;
}

}  // namespace

using testing::_;

static const uint32_t kSenderSsrc = 0x10203;
static const uint32_t kReceiverSsrc = 0x40506;
static const int kInitialReceiverClockOffsetSeconds = -5;
static const uint16_t kTargetDelayMs = 100;

class FakeRtcpTransport : public PacedPacketSender {
 public:
  explicit FakeRtcpTransport(base::SimpleTestTickClock* clock)
      : clock_(clock), packet_delay_(base::TimeDelta::FromMilliseconds(42)) {}

  void set_rtcp_destination(RtcpSession* rtcp_session) {
    rtcp_session_ = rtcp_session;
  }

  base::TimeDelta packet_delay() const { return packet_delay_; }
  void set_packet_delay(base::TimeDelta delay) { packet_delay_ = delay; }

  bool SendRtcpPacket(uint32_t ssrc, PacketRef packet) final {
    clock_->Advance(packet_delay_);
    rtcp_session_->IncomingRtcpPacket(&packet->data[0], packet->data.size());
    return true;
  }

  bool SendPackets(const SendPacketVector& packets) final { return false; }

  bool ResendPackets(const SendPacketVector& packets,
                     const DedupInfo& dedup_info) final {
    return false;
  }

  void CancelSendingPacket(const PacketKey& packet_key) final {}

 private:
  base::SimpleTestTickClock* const clock_;
  base::TimeDelta packet_delay_;
  RtcpSession* rtcp_session_;  //  RTCP destination.
  bool paused_;

  DISALLOW_COPY_AND_ASSIGN(FakeRtcpTransport);
};

class RtcpTest : public ::testing::Test {
 protected:
  RtcpTest()
      : sender_clock_(new base::SimpleTestTickClock()),
        receiver_clock_(new test::SkewedTickClock(sender_clock_.get())),
        sender_to_receiver_(sender_clock_.get()),
        receiver_to_sender_(sender_clock_.get()),
        rtcp_for_sender_(
            base::Bind(&RtcpTest::OnReceivedCastFeedback,
                       base::Unretained(this)),
            base::Bind(&RtcpTest::OnMeasuredRoundTripTime,
                       base::Unretained(this)),
            base::Bind(&RtcpTest::OnReceivedLogs, base::Unretained(this)),
            sender_clock_.get(),
            &sender_to_receiver_,
            kSenderSsrc,
            kReceiverSsrc),
        rtcp_for_receiver_(receiver_clock_.get(),
                           &receiver_to_sender_,
                           kReceiverSsrc,
                           kSenderSsrc) {
    sender_clock_->Advance(base::TimeTicks::Now() - base::TimeTicks());
    receiver_clock_->SetSkew(
        1.0,  // No skew.
        base::TimeDelta::FromSeconds(kInitialReceiverClockOffsetSeconds));

    sender_to_receiver_.set_rtcp_destination(&rtcp_for_receiver_);
    receiver_to_sender_.set_rtcp_destination(&rtcp_for_sender_);
  }

  ~RtcpTest() override {}

  void OnReceivedCastFeedback(const RtcpCastMessage& cast_message) {
    last_cast_message_ = cast_message;
  }

  void OnMeasuredRoundTripTime(base::TimeDelta rtt) {
    current_round_trip_time_ = rtt;
  }

  void OnReceivedLogs(const RtcpReceiverLogMessage& receiver_logs) {
    RtcpReceiverLogMessage().swap(last_logs_);

    // Make a copy of the logs.
    for (const RtcpReceiverFrameLogMessage& frame_log_msg : receiver_logs) {
      last_logs_.push_back(
          RtcpReceiverFrameLogMessage(frame_log_msg.rtp_timestamp_));
      for (const RtcpReceiverEventLogMessage& event_log_msg :
           frame_log_msg.event_log_messages_) {
        RtcpReceiverEventLogMessage event_log;
        event_log.type = event_log_msg.type;
        event_log.event_timestamp = event_log_msg.event_timestamp;
        event_log.delay_delta = event_log_msg.delay_delta;
        event_log.packet_id = event_log_msg.packet_id;
        last_logs_.back().event_log_messages_.push_back(event_log);
      }
    }
  }

  scoped_ptr<base::SimpleTestTickClock> sender_clock_;
  scoped_ptr<test::SkewedTickClock> receiver_clock_;
  FakeRtcpTransport sender_to_receiver_;
  FakeRtcpTransport receiver_to_sender_;
  SenderRtcpSession rtcp_for_sender_;
  ReceiverRtcpSession rtcp_for_receiver_;

  base::TimeDelta current_round_trip_time_;
  RtcpCastMessage last_cast_message_;
  RtcpReceiverLogMessage last_logs_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RtcpTest);
};

TEST_F(RtcpTest, LipSyncGleanedFromSenderReport) {
  // Initially, expect no lip-sync info receiver-side without having first
  // received a RTCP packet.
  base::TimeTicks reference_time;
  RtpTimeTicks rtp_timestamp;
  ASSERT_FALSE(rtcp_for_receiver_.GetLatestLipSyncTimes(&rtp_timestamp,
                                                        &reference_time));

  // Send a Sender Report to the receiver.
  const base::TimeTicks reference_time_sent = sender_clock_->NowTicks();
  const RtpTimeTicks rtp_timestamp_sent =
      RtpTimeTicks().Expand(UINT32_C(0xbee5));
  rtcp_for_sender_.SendRtcpReport(reference_time_sent, rtp_timestamp_sent, 1,
                                  1);

  // Now the receiver should have lip-sync info.  Confirm that the lip-sync
  // reference time is the same as that sent.
  EXPECT_TRUE(rtcp_for_receiver_.GetLatestLipSyncTimes(&rtp_timestamp,
                                                       &reference_time));
  const base::TimeTicks rolled_back_time =
      (reference_time -
       // Roll-back relative clock offset:
       base::TimeDelta::FromSeconds(kInitialReceiverClockOffsetSeconds) -
       // Roll-back packet transmission time (because RTT is not yet known):
       sender_to_receiver_.packet_delay());
  EXPECT_NEAR(0, (reference_time_sent - rolled_back_time).InMicroseconds(), 5);
  EXPECT_EQ(rtp_timestamp_sent, rtp_timestamp);
}

TEST_F(RtcpTest, RoundTripTimesDeterminedFromReportPingPong) {
  const int iterations = 12;

  // Sender does not know the RTT yet.
  ASSERT_EQ(base::TimeDelta(), rtcp_for_sender_.current_round_trip_time());

  // Do a number of ping-pongs, checking how the round trip times are measured
  // by the sender.
  base::TimeDelta expected_rtt_according_to_sender;
  for (int i = 0; i < iterations; ++i) {
    const base::TimeDelta one_way_trip_time =
        base::TimeDelta::FromMilliseconds(1 << i);
    sender_to_receiver_.set_packet_delay(one_way_trip_time);
    receiver_to_sender_.set_packet_delay(one_way_trip_time);

    // Sender --> Receiver
    base::TimeTicks reference_time_sent = sender_clock_->NowTicks();
    const RtpTimeTicks rtp_timestamp_sent =
        RtpTimeTicks().Expand<uint32_t>(0xbee5) + RtpTimeDelta::FromTicks(i);
    rtcp_for_sender_.SendRtcpReport(reference_time_sent, rtp_timestamp_sent, 1,
                                    1);
    EXPECT_EQ(expected_rtt_according_to_sender,
              rtcp_for_sender_.current_round_trip_time());

    // Validate last reported callback value is same as that reported by method.
    EXPECT_EQ(current_round_trip_time_,
              rtcp_for_sender_.current_round_trip_time());

    // Receiver --> Sender
    RtpReceiverStatistics stats;
    rtcp_for_receiver_.SendRtcpReport(
        CreateRtcpTimeData(receiver_clock_->NowTicks()), nullptr,
        base::TimeDelta(), nullptr, &stats);
    expected_rtt_according_to_sender = one_way_trip_time * 2;
    EXPECT_EQ(expected_rtt_according_to_sender,
              rtcp_for_sender_.current_round_trip_time());
  }
}

TEST_F(RtcpTest, ReportCastFeedback) {
  RtcpCastMessage cast_message(kSenderSsrc);
  cast_message.ack_frame_id = 5;
  PacketIdSet missing_packets1 = {3, 4};
  cast_message.missing_frames_and_packets[1] = missing_packets1;
  PacketIdSet missing_packets2 = {5, 6};
  cast_message.missing_frames_and_packets[2] = missing_packets2;

  rtcp_for_receiver_.SendRtcpReport(
      CreateRtcpTimeData(base::TimeTicks()), &cast_message,
      base::TimeDelta::FromMilliseconds(kTargetDelayMs), nullptr, nullptr);

  EXPECT_EQ(last_cast_message_.ack_frame_id, cast_message.ack_frame_id);
  EXPECT_EQ(last_cast_message_.target_delay_ms, kTargetDelayMs);
  EXPECT_EQ(last_cast_message_.missing_frames_and_packets.size(),
            cast_message.missing_frames_and_packets.size());
  EXPECT_TRUE(
      std::equal(cast_message.missing_frames_and_packets.begin(),
                 cast_message.missing_frames_and_packets.end(),
                 last_cast_message_.missing_frames_and_packets.begin()));
}

TEST_F(RtcpTest, DropLateRtcpPacket) {
  RtcpCastMessage cast_message(kSenderSsrc);
  cast_message.ack_frame_id = 1;
  rtcp_for_receiver_.SendRtcpReport(
      CreateRtcpTimeData(receiver_clock_->NowTicks()), &cast_message,
      base::TimeDelta::FromMilliseconds(kTargetDelayMs), nullptr, nullptr);

  // Send a packet with old timestamp
  RtcpCastMessage late_cast_message(kSenderSsrc);
  late_cast_message.ack_frame_id = 2;
  rtcp_for_receiver_.SendRtcpReport(
      CreateRtcpTimeData(receiver_clock_->NowTicks() -
                         base::TimeDelta::FromSeconds(10)),
      &late_cast_message, base::TimeDelta(), nullptr, nullptr);

  // Validate data from second packet is dropped.
  EXPECT_EQ(last_cast_message_.ack_frame_id, cast_message.ack_frame_id);
  EXPECT_EQ(last_cast_message_.target_delay_ms, kTargetDelayMs);

  // Re-send with fresh timestamp
  late_cast_message.ack_frame_id = 2;
  rtcp_for_receiver_.SendRtcpReport(
      CreateRtcpTimeData(receiver_clock_->NowTicks()), &late_cast_message,
      base::TimeDelta(), nullptr, nullptr);
  EXPECT_EQ(last_cast_message_.ack_frame_id, late_cast_message.ack_frame_id);
  EXPECT_EQ(last_cast_message_.target_delay_ms, 0);
}

TEST_F(RtcpTest, ReportReceiverEvents) {
  const RtpTimeTicks kRtpTimeStamp =
      media::cast::RtpTimeTicks().Expand(UINT32_C(100));
  const base::TimeTicks kEventTimestamp = receiver_clock_->NowTicks();
  const base::TimeDelta kDelayDelta = base::TimeDelta::FromMilliseconds(100);

  RtcpEvent event;
  event.type = FRAME_ACK_SENT;
  event.timestamp = kEventTimestamp;
  event.delay_delta = kDelayDelta;
  ReceiverRtcpEventSubscriber::RtcpEvents rtcp_events;
  rtcp_events.push_back(std::make_pair(kRtpTimeStamp, event));

  rtcp_for_receiver_.SendRtcpReport(
      CreateRtcpTimeData(receiver_clock_->NowTicks()), nullptr,
      base::TimeDelta(), &rtcp_events, nullptr);

  ASSERT_EQ(1UL, last_logs_.size());
  RtcpReceiverFrameLogMessage frame_log = last_logs_.front();
  EXPECT_EQ(frame_log.rtp_timestamp_, kRtpTimeStamp);

  ASSERT_EQ(1UL, frame_log.event_log_messages_.size());
  RtcpReceiverEventLogMessage log_msg = frame_log.event_log_messages_.back();
  EXPECT_EQ(log_msg.type, event.type);
  EXPECT_EQ(log_msg.delay_delta, event.delay_delta);
  // Only 24 bits of event timestamp sent on wire.
  uint32_t event_ts =
      (event.timestamp - base::TimeTicks()).InMilliseconds() & 0xffffff;
  uint32_t log_msg_ts =
      (log_msg.event_timestamp - base::TimeTicks()).InMilliseconds() & 0xffffff;
  EXPECT_EQ(log_msg_ts, event_ts);
}

}  // namespace cast
}  // namespace media
