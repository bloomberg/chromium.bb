// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/tick_clock.h"
#include "media/cast/cast_environment.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/rtcp/receiver_rtcp_event_subscriber.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

namespace {

const size_t kMaxEventEntries = 10u;
const int64 kDelayMs = 20L;

}  // namespace

class ReceiverRtcpEventSubscriberTest : public ::testing::Test {
 protected:
  ReceiverRtcpEventSubscriberTest()
      : testing_clock_(new base::SimpleTestTickClock()),
        task_runner_(new test::FakeSingleThreadTaskRunner(testing_clock_)),
        cast_environment_(new CastEnvironment(
            scoped_ptr<base::TickClock>(testing_clock_).Pass(), task_runner_,
            task_runner_, task_runner_, task_runner_, task_runner_,
            task_runner_, GetLoggingConfigWithRawEventsAndStatsEnabled())) {}

  virtual ~ReceiverRtcpEventSubscriberTest() {}

  virtual void TearDown() OVERRIDE {
    if (event_subscriber_) {
      cast_environment_->Logging()->RemoveRawEventSubscriber(
          event_subscriber_.get());
    }
  }

  void Init(ReceiverRtcpEventSubscriber::Type type) {
    event_subscriber_.reset(
        new ReceiverRtcpEventSubscriber(kMaxEventEntries, type));
    cast_environment_->Logging()->AddRawEventSubscriber(
        event_subscriber_.get());
  }

  void InsertEvents() {
    // Video events
    cast_environment_->Logging()->InsertFrameEventWithDelay(
        testing_clock_->NowTicks(), kVideoRenderDelay, /*rtp_timestamp*/ 100u,
        /*frame_id*/ 2u, base::TimeDelta::FromMilliseconds(kDelayMs));
    cast_environment_->Logging()->InsertFrameEvent(
        testing_clock_->NowTicks(), kVideoFrameDecoded, /*rtp_timestamp*/ 200u,
        /*frame_id*/ 1u);
    cast_environment_->Logging()->InsertPacketEvent(
        testing_clock_->NowTicks(), kVideoPacketReceived,
        /*rtp_timestamp */ 200u, /*frame_id*/ 2u, /*packet_id*/ 1u,
        /*max_packet_id*/ 10u, /*size*/ 1024u);

    // Audio events
    cast_environment_->Logging()->InsertFrameEventWithDelay(
        testing_clock_->NowTicks(), kAudioPlayoutDelay, /*rtp_timestamp*/ 300u,
        /*frame_id*/ 4u, base::TimeDelta::FromMilliseconds(kDelayMs));
    cast_environment_->Logging()->InsertFrameEvent(
        testing_clock_->NowTicks(), kAudioFrameDecoded, /*rtp_timestamp*/ 400u,
        /*frame_id*/ 3u);
    cast_environment_->Logging()->InsertPacketEvent(
        testing_clock_->NowTicks(), kAudioPacketReceived,
        /*rtp_timestamp */ 400u, /*frame_id*/ 5u, /*packet_id*/ 1u,
        /*max_packet_id*/ 10u, /*size*/ 128u);

    // Unrelated events
    cast_environment_->Logging()->InsertFrameEvent(
        testing_clock_->NowTicks(), kVideoFrameReceived, /*rtp_timestamp*/ 100u,
        /*frame_id*/ 1u);
    cast_environment_->Logging()->InsertFrameEvent(
        testing_clock_->NowTicks(), kAudioFrameReceived, /*rtp_timestamp*/ 100u,
        /*frame_id*/ 1u);
    cast_environment_->Logging()->InsertGenericEvent(testing_clock_->NowTicks(),
                                                     kRttMs, /*value*/ 100);
  }

  base::SimpleTestTickClock* testing_clock_;  // Owned by CastEnvironment.
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;
  scoped_ptr<ReceiverRtcpEventSubscriber> event_subscriber_;
};

TEST_F(ReceiverRtcpEventSubscriberTest, LogVideoEvents) {
  Init(ReceiverRtcpEventSubscriber::kVideoEventSubscriber);

  InsertEvents();

  RtcpReceiverLogMessage receiver_log;
  event_subscriber_->GetReceiverLogMessageAndReset(&receiver_log);

  ASSERT_EQ(2u, receiver_log.size());
  RtcpReceiverLogMessage::iterator log_it = receiver_log.begin();
  EXPECT_EQ(100u, log_it->rtp_timestamp_);
  ASSERT_EQ(1u, log_it->event_log_messages_.size());
  RtcpReceiverEventLogMessages::iterator event_it =
      log_it->event_log_messages_.begin();
  EXPECT_EQ(kVideoRenderDelay, event_it->type);
  EXPECT_EQ(kDelayMs, event_it->delay_delta.InMilliseconds());

  ++log_it;
  EXPECT_EQ(200u, log_it->rtp_timestamp_);
  ASSERT_EQ(2u, log_it->event_log_messages_.size());
}

TEST_F(ReceiverRtcpEventSubscriberTest, LogAudioEvents) {
  Init(ReceiverRtcpEventSubscriber::kAudioEventSubscriber);

  InsertEvents();

  RtcpReceiverLogMessage receiver_log;
  event_subscriber_->GetReceiverLogMessageAndReset(&receiver_log);

  ASSERT_EQ(2u, receiver_log.size());
  RtcpReceiverLogMessage::iterator log_it = receiver_log.begin();
  EXPECT_EQ(300u, log_it->rtp_timestamp_);
  ASSERT_EQ(1u, log_it->event_log_messages_.size());
  RtcpReceiverEventLogMessages::iterator event_it =
      log_it->event_log_messages_.begin();
  EXPECT_EQ(kAudioPlayoutDelay, event_it->type);
  EXPECT_EQ(kDelayMs, event_it->delay_delta.InMilliseconds());

  ++log_it;
  EXPECT_EQ(400u, log_it->rtp_timestamp_);
  ASSERT_EQ(2u, log_it->event_log_messages_.size());
}

TEST_F(ReceiverRtcpEventSubscriberTest, MapReset) {
  Init(ReceiverRtcpEventSubscriber::kVideoEventSubscriber);

  cast_environment_->Logging()->InsertFrameEvent(
      testing_clock_->NowTicks(), kVideoFrameDecoded, /*rtp_timestamp*/ 100u,
      /*frame_id*/ 1u);

  RtcpReceiverLogMessage receiver_log;
  event_subscriber_->GetReceiverLogMessageAndReset(&receiver_log);

  EXPECT_EQ(1u, receiver_log.size());

  // Call again without any logging in between, should return empty log.
  event_subscriber_->GetReceiverLogMessageAndReset(&receiver_log);
  EXPECT_TRUE(receiver_log.empty());
}

TEST_F(ReceiverRtcpEventSubscriberTest, DropEventsWhenSizeExceeded) {
  Init(ReceiverRtcpEventSubscriber::kVideoEventSubscriber);

  for (uint32 i = 1u; i <= 10u; ++i) {
    cast_environment_->Logging()->InsertFrameEvent(
        testing_clock_->NowTicks(), kVideoFrameDecoded,
        /*rtp_timestamp*/ i * 10, /*frame_id*/ i);
  }

  RtcpReceiverLogMessage receiver_log;
  event_subscriber_->GetReceiverLogMessageAndReset(&receiver_log);

  ASSERT_EQ(10u, receiver_log.size());
  EXPECT_EQ(10u, receiver_log.begin()->rtp_timestamp_);
  EXPECT_EQ(100u, receiver_log.rbegin()->rtp_timestamp_);

  for (uint32 i = 1u; i <= 11u; ++i) {
    cast_environment_->Logging()->InsertFrameEvent(
        testing_clock_->NowTicks(), kVideoFrameDecoded,
        /*rtp_timestamp*/ i * 10, /*frame_id*/ i);
  }

  event_subscriber_->GetReceiverLogMessageAndReset(&receiver_log);

  // Event with RTP timestamp 10 should have been dropped when 110 is inserted.
  ASSERT_EQ(10u, receiver_log.size());
  EXPECT_EQ(20u, receiver_log.begin()->rtp_timestamp_);
  EXPECT_EQ(110u, receiver_log.rbegin()->rtp_timestamp_);
}

}  // namespace cast
}  // namespace media
