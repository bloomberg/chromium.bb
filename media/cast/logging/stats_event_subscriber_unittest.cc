// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/tick_clock.h"
#include "media/cast/cast_environment.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/stats_event_subscriber.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

class StatsEventSubscriberTest : public ::testing::Test {
 protected:
  StatsEventSubscriberTest()
      : testing_clock_(new base::SimpleTestTickClock()),
        task_runner_(new test::FakeSingleThreadTaskRunner(testing_clock_)),
        cast_environment_(new CastEnvironment(
            scoped_ptr<base::TickClock>(testing_clock_).Pass(),
            task_runner_,
            task_runner_,
            task_runner_)) {}

  virtual ~StatsEventSubscriberTest() {
    if (subscriber_.get())
      cast_environment_->Logging()->RemoveRawEventSubscriber(subscriber_.get());
  }

  void Init(EventMediaType event_media_type) {
    DCHECK(!subscriber_.get());
    subscriber_.reset(new StatsEventSubscriber(event_media_type));
    cast_environment_->Logging()->AddRawEventSubscriber(subscriber_.get());
  }

  base::SimpleTestTickClock* testing_clock_;  // Owned by CastEnvironment.
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;
  scoped_ptr<StatsEventSubscriber> subscriber_;
};

TEST_F(StatsEventSubscriberTest, FrameStats) {
  Init(VIDEO_EVENT);
  uint32 rtp_timestamp = 0;
  uint32 frame_id = 0;
  int num_frames = 10;
  int frame_size = 123;
  int delay_base_ms = 10;
  base::TimeTicks now;
  for (int i = 0; i < num_frames; i++) {
    now = testing_clock_->NowTicks();
    cast_environment_->Logging()->InsertFrameEvent(
        now, kVideoFrameReceived, rtp_timestamp, frame_id);
    testing_clock_->Advance(base::TimeDelta::FromMilliseconds(30));

    cast_environment_->Logging()->InsertEncodedFrameEvent(
        now, kVideoFrameEncoded, rtp_timestamp, i, frame_size, true);
    testing_clock_->Advance(base::TimeDelta::FromMilliseconds(30));

    cast_environment_->Logging()->InsertFrameEventWithDelay(
        now,
        kVideoRenderDelay,
        rtp_timestamp,
        i,
        base::TimeDelta::FromMilliseconds(i * delay_base_ms));
    testing_clock_->Advance(base::TimeDelta::FromMilliseconds(30));

    rtp_timestamp += 90;
  }

  // Verify stats.
  FrameStatsMap frame_stats;
  subscriber_->GetFrameStats(&frame_stats);

  // Size of stats equals the number of events.
  EXPECT_EQ(3u, frame_stats.size());
  FrameStatsMap::const_iterator it = frame_stats.find(kVideoFrameReceived);
  ASSERT_TRUE(it != frame_stats.end());
  EXPECT_EQ(num_frames, it->second.event_counter);

  it = frame_stats.find(kVideoFrameEncoded);
  ASSERT_TRUE(it != frame_stats.end());

  EXPECT_EQ(num_frames * frame_size, static_cast<int>(it->second.sum_size));

  it = frame_stats.find(kVideoRenderDelay);
  ASSERT_TRUE(it != frame_stats.end());

  EXPECT_EQ(0, it->second.min_delay.InMilliseconds());
  EXPECT_EQ((num_frames - 1) * delay_base_ms,
            it->second.max_delay.InMilliseconds());
  EXPECT_EQ((num_frames - 1) * num_frames / 2 * delay_base_ms,
            it->second.sum_delay.InMilliseconds());
}

TEST_F(StatsEventSubscriberTest, PacketStats) {
  Init(AUDIO_EVENT);
  uint32 rtp_timestamp = 0;
  uint32 frame_id = 0;
  int num_packets = 10;
  int packet_size = 123;
  base::TimeTicks first_event_time = testing_clock_->NowTicks();
  base::TimeTicks now;
  for (int i = 0; i < num_packets; i++) {
    now = testing_clock_->NowTicks();
    cast_environment_->Logging()->InsertPacketEvent(now,
                                                    kAudioPacketSentToPacer,
                                                    rtp_timestamp,
                                                    frame_id,
                                                    0,
                                                    10,
                                                    packet_size);
    testing_clock_->Advance(base::TimeDelta::FromMilliseconds(30));
  }

  PacketStatsMap stats_map;
  subscriber_->GetPacketStats(&stats_map);

  // Size of stats equals the number of event types.
  EXPECT_EQ(1u, stats_map.size());
  PacketStatsMap::const_iterator it = stats_map.find(kAudioPacketSentToPacer);
  ASSERT_NE(stats_map.end(), it);

  EXPECT_EQ(first_event_time, it->second.first_event_time);
  EXPECT_EQ(now, it->second.last_event_time);
  EXPECT_EQ(num_packets, it->second.event_counter);
  EXPECT_EQ(num_packets * packet_size, static_cast<int>(it->second.sum_size));
}

TEST_F(StatsEventSubscriberTest, GenericStats) {
  Init(OTHER_EVENT);
  int num_generic_events = 10;
  int value = 123;
  for (int i = 0; i < num_generic_events; i++) {
    cast_environment_->Logging()->InsertGenericEvent(
        testing_clock_->NowTicks(), kRttMs, value);
  }

  GenericStatsMap stats_map;
  subscriber_->GetGenericStats(&stats_map);

  EXPECT_EQ(1u, stats_map.size());
  GenericStatsMap::const_iterator it = stats_map.find(kRttMs);
  ASSERT_NE(stats_map.end(), it);

  EXPECT_EQ(num_generic_events, it->second.event_counter);
  EXPECT_EQ(num_generic_events * value, it->second.sum);
  EXPECT_EQ(static_cast<uint64>(num_generic_events * value * value),
            it->second.sum_squared);
  EXPECT_LE(value, it->second.min);
  EXPECT_GE(value, it->second.max);
}

TEST_F(StatsEventSubscriberTest, Reset) {
  Init(VIDEO_EVENT);
  cast_environment_->Logging()->InsertFrameEvent(
      testing_clock_->NowTicks(), kVideoFrameReceived, 0, 0);

  FrameStatsMap frame_stats;
  subscriber_->GetFrameStats(&frame_stats);
  EXPECT_EQ(1u, frame_stats.size());

  subscriber_->Reset();
  subscriber_->GetFrameStats(&frame_stats);
  EXPECT_TRUE(frame_stats.empty());
}

}  // namespace cast
}  // namespace media
