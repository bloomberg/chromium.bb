// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/test/simple_test_tick_clock.h"
#include "media/cast/cast_defines.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/net/pacing/paced_sender.h"
#include "media/cast/net/rtcp/rtcp.h"
#include "media/cast/test/skewed_tick_clock.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

using testing::_;

static const uint32 kSenderSsrc = 0x10203;
static const uint32 kReceiverSsrc = 0x40506;
static const int kInitialReceiverClockOffsetSeconds = -5;

class FakeRtcpTransport : public PacedPacketSender {
 public:
  explicit FakeRtcpTransport(base::SimpleTestTickClock* clock)
      : clock_(clock),
        packet_delay_(base::TimeDelta::FromMilliseconds(42)) {}

  void set_rtcp_destination(Rtcp* rtcp) { rtcp_ = rtcp; }

  base::TimeDelta packet_delay() const { return packet_delay_; }
  void set_packet_delay(base::TimeDelta delay) { packet_delay_ = delay; }

  virtual bool SendRtcpPacket(uint32 ssrc, PacketRef packet) OVERRIDE {
    clock_->Advance(packet_delay_);
    rtcp_->IncomingRtcpPacket(&packet->data[0], packet->data.size());
    return true;
  }

  virtual bool SendPackets(const SendPacketVector& packets) OVERRIDE {
    return false;
  }

  virtual bool ResendPackets(
      const SendPacketVector& packets, const DedupInfo& dedup_info) OVERRIDE {
    return false;
  }

  virtual void CancelSendingPacket(const PacketKey& packet_key) OVERRIDE {
  }

 private:
  base::SimpleTestTickClock* const clock_;
  base::TimeDelta packet_delay_;
  Rtcp* rtcp_;

  DISALLOW_COPY_AND_ASSIGN(FakeRtcpTransport);
};

class FakeReceiverStats : public RtpReceiverStatistics {
 public:
  FakeReceiverStats() {}
  virtual ~FakeReceiverStats() {}

  virtual void GetStatistics(uint8* fraction_lost,
                             uint32* cumulative_lost,
                             uint32* extended_high_sequence_number,
                             uint32* jitter) OVERRIDE {
    *fraction_lost = 0;
    *cumulative_lost = 0;
    *extended_high_sequence_number = 0;
    *jitter = 0;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeReceiverStats);
};

class MockFrameSender {
 public:
  MockFrameSender() {}
  virtual ~MockFrameSender() {}

  MOCK_METHOD1(OnReceivedCastFeedback,
               void(const RtcpCastMessage& cast_message));
  MOCK_METHOD1(OnMeasuredRoundTripTime, void(base::TimeDelta rtt));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockFrameSender);
};

class RtcpTest : public ::testing::Test {
 protected:
  RtcpTest()
      : sender_clock_(new base::SimpleTestTickClock()),
        receiver_clock_(new test::SkewedTickClock(sender_clock_.get())),
        sender_to_receiver_(sender_clock_.get()),
        receiver_to_sender_(sender_clock_.get()),
        rtcp_for_sender_(base::Bind(&MockFrameSender::OnReceivedCastFeedback,
                                    base::Unretained(&mock_frame_sender_)),
                         base::Bind(&MockFrameSender::OnMeasuredRoundTripTime,
                                    base::Unretained(&mock_frame_sender_)),
                         RtcpLogMessageCallback(),
                         sender_clock_.get(),
                         &sender_to_receiver_,
                         kSenderSsrc,
                         kReceiverSsrc),
        rtcp_for_receiver_(RtcpCastMessageCallback(),
                           RtcpRttCallback(),
                           RtcpLogMessageCallback(),
                           receiver_clock_.get(),
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

  virtual ~RtcpTest() {}

  scoped_ptr<base::SimpleTestTickClock> sender_clock_;
  scoped_ptr<test::SkewedTickClock> receiver_clock_;
  FakeRtcpTransport sender_to_receiver_;
  FakeRtcpTransport receiver_to_sender_;
  MockFrameSender mock_frame_sender_;
  Rtcp rtcp_for_sender_;
  Rtcp rtcp_for_receiver_;
  FakeReceiverStats stats_;

  DISALLOW_COPY_AND_ASSIGN(RtcpTest);
};

TEST_F(RtcpTest, LipSyncGleanedFromSenderReport) {
  // Initially, expect no lip-sync info receiver-side without having first
  // received a RTCP packet.
  base::TimeTicks reference_time;
  uint32 rtp_timestamp;
  ASSERT_FALSE(rtcp_for_receiver_.GetLatestLipSyncTimes(&rtp_timestamp,
                                                        &reference_time));

  // Send a Sender Report to the receiver.
  const base::TimeTicks reference_time_sent = sender_clock_->NowTicks();
  const uint32 rtp_timestamp_sent = 0xbee5;
  rtcp_for_sender_.SendRtcpFromRtpSender(
      reference_time_sent, rtp_timestamp_sent, 1, 1);

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

// TODO(miu): There were a few tests here that didn't actually test anything
// except that the code wouldn't crash and a callback method was invoked.  We
// need to fill-in more testing of RTCP now that much of the refactoring work
// has been completed.

TEST_F(RtcpTest, RoundTripTimesDeterminedFromReportPingPong) {
  const int iterations = 12;
  EXPECT_CALL(mock_frame_sender_, OnMeasuredRoundTripTime(_))
      .Times(iterations);

  // Initially, neither side knows the round trip time.
  ASSERT_EQ(base::TimeDelta(), rtcp_for_sender_.current_round_trip_time());
  ASSERT_EQ(base::TimeDelta(), rtcp_for_receiver_.current_round_trip_time());

  // Do a number of ping-pongs, checking how the round trip times are measured
  // by the sender and receiver.
  base::TimeDelta expected_rtt_according_to_sender;
  base::TimeDelta expected_rtt_according_to_receiver;
  for (int i = 0; i < iterations; ++i) {
    const base::TimeDelta one_way_trip_time =
        base::TimeDelta::FromMilliseconds(1 << i);
    sender_to_receiver_.set_packet_delay(one_way_trip_time);
    receiver_to_sender_.set_packet_delay(one_way_trip_time);

    // Sender --> Receiver
    base::TimeTicks reference_time_sent = sender_clock_->NowTicks();
    uint32 rtp_timestamp_sent = 0xbee5 + i;
    rtcp_for_sender_.SendRtcpFromRtpSender(
        reference_time_sent, rtp_timestamp_sent, 1, 1);
    EXPECT_EQ(expected_rtt_according_to_sender,
              rtcp_for_sender_.current_round_trip_time());
#ifdef SENDER_PROVIDES_REPORT_BLOCK
    EXPECT_EQ(expected_rtt_according_to_receiver,
              rtcp_for_receiver_.current_round_trip_time());
#endif

    // Receiver --> Sender
    rtcp_for_receiver_.SendRtcpFromRtpReceiver(
        NULL, base::TimeDelta(), NULL, &stats_);
    expected_rtt_according_to_sender = one_way_trip_time * 2;
    EXPECT_EQ(expected_rtt_according_to_sender,
              rtcp_for_sender_.current_round_trip_time());
#ifdef SENDER_PROVIDES_REPORT_BLOCK
    EXPECT_EQ(expected_rtt_according_to_receiver,
              rtcp_for_receiver_.current_round_trip_time();
#endif

    // In the next iteration of this loop, after the receiver gets the sender
    // report, it will be measuring a round trip time consisting of two
    // different one-way trip times.
    expected_rtt_according_to_receiver =
        (one_way_trip_time + one_way_trip_time * 2) / 2;
  }
}

// TODO(miu): Find a better home for this test.
TEST(MisplacedCastTest, NtpAndTime) {
  const int64 kSecondsbetweenYear1900and2010 = INT64_C(40176 * 24 * 60 * 60);
  const int64 kSecondsbetweenYear1900and2030 = INT64_C(47481 * 24 * 60 * 60);

  uint32 ntp_seconds_1 = 0;
  uint32 ntp_fraction_1 = 0;
  base::TimeTicks input_time = base::TimeTicks::Now();
  ConvertTimeTicksToNtp(input_time, &ntp_seconds_1, &ntp_fraction_1);

  // Verify absolute value.
  EXPECT_GT(ntp_seconds_1, kSecondsbetweenYear1900and2010);
  EXPECT_LT(ntp_seconds_1, kSecondsbetweenYear1900and2030);

  base::TimeTicks out_1 = ConvertNtpToTimeTicks(ntp_seconds_1, ntp_fraction_1);
  EXPECT_EQ(input_time, out_1);  // Verify inverse.

  base::TimeDelta time_delta = base::TimeDelta::FromMilliseconds(1000);
  input_time += time_delta;

  uint32 ntp_seconds_2 = 0;
  uint32 ntp_fraction_2 = 0;

  ConvertTimeTicksToNtp(input_time, &ntp_seconds_2, &ntp_fraction_2);
  base::TimeTicks out_2 = ConvertNtpToTimeTicks(ntp_seconds_2, ntp_fraction_2);
  EXPECT_EQ(input_time, out_2);  // Verify inverse.

  // Verify delta.
  EXPECT_EQ((out_2 - out_1), time_delta);
  EXPECT_EQ((ntp_seconds_2 - ntp_seconds_1), UINT32_C(1));
  EXPECT_NEAR(ntp_fraction_2, ntp_fraction_1, 1);

  time_delta = base::TimeDelta::FromMilliseconds(500);
  input_time += time_delta;

  uint32 ntp_seconds_3 = 0;
  uint32 ntp_fraction_3 = 0;

  ConvertTimeTicksToNtp(input_time, &ntp_seconds_3, &ntp_fraction_3);
  base::TimeTicks out_3 = ConvertNtpToTimeTicks(ntp_seconds_3, ntp_fraction_3);
  EXPECT_EQ(input_time, out_3);  // Verify inverse.

  // Verify delta.
  EXPECT_EQ((out_3 - out_2), time_delta);
  EXPECT_NEAR((ntp_fraction_3 - ntp_fraction_2), 0xffffffff / 2, 1);
}

}  // namespace cast
}  // namespace media
