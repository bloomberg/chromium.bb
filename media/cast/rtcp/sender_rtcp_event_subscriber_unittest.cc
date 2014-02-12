// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/tick_clock.h"
#include "media/cast/cast_environment.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/rtcp/sender_rtcp_event_subscriber.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

namespace {

const size_t kMaxEventEntries = 10u;

}  // namespace

class SenderRtcpEventSubscriberTest : public ::testing::Test {
 protected:
  SenderRtcpEventSubscriberTest()
      : testing_clock_(new base::SimpleTestTickClock()),
        task_runner_(new test::FakeSingleThreadTaskRunner(testing_clock_)),
        cast_environment_(new CastEnvironment(
            scoped_ptr<base::TickClock>(testing_clock_).Pass(), task_runner_,
            task_runner_, task_runner_, task_runner_, task_runner_,
            task_runner_, GetLoggingConfigWithRawEventsAndStatsEnabled())),
        event_subscriber_(kMaxEventEntries) {
    cast_environment_->Logging()->AddRawEventSubscriber(&event_subscriber_);
  }

  virtual ~SenderRtcpEventSubscriberTest() {
    cast_environment_->Logging()->RemoveRawEventSubscriber(&event_subscriber_);
  }

  base::SimpleTestTickClock* testing_clock_;  // Owned by CastEnvironment.
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;
  SenderRtcpEventSubscriber event_subscriber_;
};

TEST_F(SenderRtcpEventSubscriberTest, InsertEntry) {
  cast_environment_->Logging()->InsertFrameEvent(testing_clock_->NowTicks(),
                                                 kVideoFrameCaptured, 100u, 1u);
  cast_environment_->Logging()->InsertFrameEvent(testing_clock_->NowTicks(),
                                                 kVideoFrameCaptured, 200u, 2u);
  cast_environment_->Logging()->InsertFrameEvent(
      testing_clock_->NowTicks(), kVideoFrameSentToEncoder, 100u, 1u);
  cast_environment_->Logging()->InsertFrameEvent(testing_clock_->NowTicks(),
                                                 kVideoFrameEncoded, 100u, 1u);
  cast_environment_->Logging()->InsertFrameEvent(testing_clock_->NowTicks(),
                                                 kVideoFrameEncoded, 300u, 3u);
  cast_environment_->Logging()->InsertFrameEvent(
      testing_clock_->NowTicks(), kVideoFrameSentToEncoder, 300u, 3u);

  RtcpEventMap events;
  event_subscriber_.GetRtcpEventsAndReset(&events);

  ASSERT_EQ(3u, events.size());

  RtcpEventMap::iterator it = events.begin();
  EXPECT_EQ(100u, it->first);
  EXPECT_EQ(kVideoFrameEncoded, it->second.type);

  ++it;
  EXPECT_EQ(200u, it->first);
  EXPECT_EQ(kVideoFrameCaptured, it->second.type);

  ++it;
  EXPECT_EQ(300u, it->first);
  EXPECT_EQ(kVideoFrameEncoded, it->second.type);
}

TEST_F(SenderRtcpEventSubscriberTest, MapReset) {
  cast_environment_->Logging()->InsertFrameEvent(testing_clock_->NowTicks(),
                                                 kVideoFrameCaptured, 100u, 1u);

  RtcpEventMap events;
  event_subscriber_.GetRtcpEventsAndReset(&events);
  EXPECT_EQ(1u, events.size());

  // Call again without any logging in between, should return empty map.
  event_subscriber_.GetRtcpEventsAndReset(&events);
  EXPECT_TRUE(events.empty());
}

TEST_F(SenderRtcpEventSubscriberTest, DropEventsWhenSizeExceeded) {
  for (uint32 i = 1u; i <= 10u; ++i) {
    cast_environment_->Logging()->InsertFrameEvent(
        testing_clock_->NowTicks(), kVideoFrameCaptured, i * 10, i);
  }

  RtcpEventMap events;
  event_subscriber_.GetRtcpEventsAndReset(&events);

  ASSERT_EQ(10u, events.size());
  EXPECT_EQ(10u, events.begin()->first);
  EXPECT_EQ(100u, events.rbegin()->first);

  for (uint32 i = 1u; i <= 11u; ++i) {
    cast_environment_->Logging()->InsertFrameEvent(
        testing_clock_->NowTicks(), kVideoFrameCaptured, i * 10, i);
  }

  event_subscriber_.GetRtcpEventsAndReset(&events);

  // Event with RTP timestamp 10 should have been dropped when 110 is inserted.
  ASSERT_EQ(10u, events.size());
  EXPECT_EQ(20u, events.begin()->first);
  EXPECT_EQ(110u, events.rbegin()->first);
}

}  // namespace cast
}  // namespace media
