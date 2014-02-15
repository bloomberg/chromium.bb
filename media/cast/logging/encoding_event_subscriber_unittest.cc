// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/tick_clock.h"
#include "media/cast/cast_environment.h"
#include "media/cast/logging/encoding_event_subscriber.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::cast::proto::AggregatedFrameEvent;
using media::cast::proto::AggregatedPacketEvent;
using media::cast::proto::BasePacketEvent;

namespace media {
namespace cast {

class EncodingEventSubscriberTest : public ::testing::Test {
 protected:
  EncodingEventSubscriberTest()
      : testing_clock_(new base::SimpleTestTickClock()),
        task_runner_(new test::FakeSingleThreadTaskRunner(testing_clock_)),
        cast_environment_(new CastEnvironment(
            scoped_ptr<base::TickClock>(testing_clock_).Pass(),
            task_runner_,
            task_runner_,
            task_runner_,
            task_runner_,
            task_runner_,
            task_runner_,
            GetLoggingConfigWithRawEventsAndStatsEnabled())) {}

  void Init(EventMediaType event_media_type) {
    DCHECK(!event_subscriber_);
    event_subscriber_.reset(new EncodingEventSubscriber(event_media_type, 10));
    cast_environment_->Logging()->AddRawEventSubscriber(
        event_subscriber_.get());
  }

  virtual ~EncodingEventSubscriberTest() {
    if (event_subscriber_) {
      cast_environment_->Logging()->RemoveRawEventSubscriber(
          event_subscriber_.get());
    }
  }

  base::SimpleTestTickClock* testing_clock_;  // Owned by CastEnvironment.
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;
  scoped_ptr<EncodingEventSubscriber> event_subscriber_;
};

TEST_F(EncodingEventSubscriberTest, FrameEventTruncating) {
  Init(VIDEO_EVENT);

  base::TimeTicks now(testing_clock_->NowTicks());

  // Entry with RTP timestamp 0 should get dropped.
  for (int i = 0; i < 11; i++) {
    cast_environment_->Logging()->InsertFrameEvent(now,
                                                   kVideoFrameCaptured,
                                                   i * 100,
                                                   /*frame_id*/ 0);
    cast_environment_->Logging()->InsertFrameEvent(now,
                                                   kVideoFrameDecoded,
                                                   i * 100,
                                                   /*frame_id*/ 0);
  }

  FrameEventMap frame_events;
  event_subscriber_->GetFrameEventsAndReset(&frame_events);

  ASSERT_EQ(10u, frame_events.size());
  EXPECT_EQ(100u, frame_events.begin()->first);
  EXPECT_EQ(1000u, frame_events.rbegin()->first);
}

TEST_F(EncodingEventSubscriberTest, PacketEventTruncating) {
  Init(AUDIO_EVENT);

  base::TimeTicks now(testing_clock_->NowTicks());

  // Entry with RTP timestamp 0 should get dropped.
  for (int i = 0; i < 11; i++) {
    cast_environment_->Logging()->InsertPacketEvent(now,
                                                    kAudioPacketReceived,
                                                    /*rtp_timestamp*/ i * 100,
                                                    /*frame_id*/ 0,
                                                    /*packet_id*/ i,
                                                    /*max_packet_id*/ 10,
                                                    /*size*/ 123);
  }

  PacketEventMap packet_events;
  event_subscriber_->GetPacketEventsAndReset(&packet_events);

  ASSERT_EQ(10u, packet_events.size());
  EXPECT_EQ(100u, packet_events.begin()->first);
  EXPECT_EQ(1000u, packet_events.rbegin()->first);
}

TEST_F(EncodingEventSubscriberTest, EventFiltering) {
  Init(VIDEO_EVENT);

  base::TimeTicks now(testing_clock_->NowTicks());
  RtpTimestamp rtp_timestamp = 100;
  cast_environment_->Logging()->InsertFrameEvent(now,
                                                 kVideoFrameDecoded,
                                                 rtp_timestamp,
                                                 /*frame_id*/ 0);

  // This is an AUDIO_EVENT and shouldn't be processed by the subscriber.
  cast_environment_->Logging()->InsertFrameEvent(now,
                                                 kAudioFrameDecoded,
                                                 rtp_timestamp,
                                                 /*frame_id*/ 0);

  FrameEventMap frame_events;
  event_subscriber_->GetFrameEventsAndReset(&frame_events);

  FrameEventMap::iterator frame_it = frame_events.find(rtp_timestamp);
  ASSERT_TRUE(frame_it != frame_events.end());

  linked_ptr<AggregatedFrameEvent> frame_event = frame_it->second;

  ASSERT_EQ(1, frame_event->event_type_size());
  EXPECT_EQ(media::cast::proto::VIDEO_FRAME_DECODED,
            frame_event->event_type(0));

  PacketEventMap packet_events;
  event_subscriber_->GetPacketEventsAndReset(&packet_events);
  EXPECT_TRUE(packet_events.empty());
}

TEST_F(EncodingEventSubscriberTest, FrameEvent) {
  Init(VIDEO_EVENT);
  base::TimeTicks now(testing_clock_->NowTicks());
  RtpTimestamp rtp_timestamp = 100;
  cast_environment_->Logging()->InsertFrameEvent(now, kVideoFrameDecoded,
                                                 rtp_timestamp,
                                                 /*frame_id*/ 0);

  FrameEventMap frame_events;
  event_subscriber_->GetFrameEventsAndReset(&frame_events);

  ASSERT_EQ(1u, frame_events.size());

  FrameEventMap::iterator it = frame_events.find(rtp_timestamp);
  ASSERT_TRUE(it != frame_events.end());

  linked_ptr<AggregatedFrameEvent> event = it->second;

  EXPECT_EQ(rtp_timestamp, event->rtp_timestamp());

  ASSERT_EQ(1, event->event_type_size());
  EXPECT_EQ(media::cast::proto::VIDEO_FRAME_DECODED, event->event_type(0));
  ASSERT_EQ(1, event->event_timestamp_micros_size());
  EXPECT_EQ(now.ToInternalValue(), event->event_timestamp_micros(0));

  EXPECT_EQ(0, event->encoded_frame_size());
  EXPECT_EQ(0, event->delay_millis());

  event_subscriber_->GetFrameEventsAndReset(&frame_events);
  EXPECT_TRUE(frame_events.empty());
}

TEST_F(EncodingEventSubscriberTest, FrameEventDelay) {
  Init(AUDIO_EVENT);
  base::TimeTicks now(testing_clock_->NowTicks());
  RtpTimestamp rtp_timestamp = 100;
  int delay_ms = 100;
  cast_environment_->Logging()->InsertFrameEventWithDelay(
      now, kAudioPlayoutDelay, rtp_timestamp,
      /*frame_id*/ 0, base::TimeDelta::FromMilliseconds(delay_ms));

  FrameEventMap frame_events;
  event_subscriber_->GetFrameEventsAndReset(&frame_events);

  ASSERT_EQ(1u, frame_events.size());

  FrameEventMap::iterator it = frame_events.find(rtp_timestamp);
  ASSERT_TRUE(it != frame_events.end());

  linked_ptr<AggregatedFrameEvent> event = it->second;

  EXPECT_EQ(rtp_timestamp, event->rtp_timestamp());

  ASSERT_EQ(1, event->event_type_size());
  EXPECT_EQ(media::cast::proto::AUDIO_PLAYOUT_DELAY, event->event_type(0));
  ASSERT_EQ(1, event->event_timestamp_micros_size());
  EXPECT_EQ(now.ToInternalValue(), event->event_timestamp_micros(0));

  EXPECT_EQ(0, event->encoded_frame_size());
  EXPECT_EQ(100, event->delay_millis());
}

TEST_F(EncodingEventSubscriberTest, FrameEventSize) {
  Init(VIDEO_EVENT);
  base::TimeTicks now(testing_clock_->NowTicks());
  RtpTimestamp rtp_timestamp = 100;
  int size = 123;
  cast_environment_->Logging()->InsertFrameEventWithSize(
      now, kVideoFrameEncoded, rtp_timestamp,
      /*frame_id*/ 0, size);

  FrameEventMap frame_events;
  event_subscriber_->GetFrameEventsAndReset(&frame_events);

  ASSERT_EQ(1u, frame_events.size());

  FrameEventMap::iterator it = frame_events.find(rtp_timestamp);
  ASSERT_TRUE(it != frame_events.end());

  linked_ptr<AggregatedFrameEvent> event = it->second;

  EXPECT_EQ(rtp_timestamp, event->rtp_timestamp());

  ASSERT_EQ(1, event->event_type_size());
  EXPECT_EQ(media::cast::proto::VIDEO_FRAME_ENCODED, event->event_type(0));
  ASSERT_EQ(1, event->event_timestamp_micros_size());
  EXPECT_EQ(now.ToInternalValue(), event->event_timestamp_micros(0));

  EXPECT_EQ(size, event->encoded_frame_size());
  EXPECT_EQ(0, event->delay_millis());
}

TEST_F(EncodingEventSubscriberTest, MultipleFrameEvents) {
  Init(AUDIO_EVENT);
  RtpTimestamp rtp_timestamp1 = 100;
  RtpTimestamp rtp_timestamp2 = 200;
  base::TimeTicks now1(testing_clock_->NowTicks());
  cast_environment_->Logging()->InsertFrameEventWithDelay(
      now1, kAudioPlayoutDelay, rtp_timestamp1,
      /*frame_id*/ 0, /*delay*/ base::TimeDelta::FromMilliseconds(100));

  testing_clock_->Advance(base::TimeDelta::FromMilliseconds(20));
  base::TimeTicks now2(testing_clock_->NowTicks());
  cast_environment_->Logging()->InsertFrameEventWithSize(
      now2, kAudioFrameEncoded, rtp_timestamp2,
      /*frame_id*/ 0, /*size*/ 123);

  testing_clock_->Advance(base::TimeDelta::FromMilliseconds(20));
  base::TimeTicks now3(testing_clock_->NowTicks());
  cast_environment_->Logging()->InsertFrameEvent(
      now3, kAudioFrameDecoded, rtp_timestamp1, /*frame_id*/ 0);

  FrameEventMap frame_events;
  event_subscriber_->GetFrameEventsAndReset(&frame_events);

  ASSERT_EQ(2u, frame_events.size());

  FrameEventMap::iterator it = frame_events.find(100);
  ASSERT_TRUE(it != frame_events.end());

  linked_ptr<AggregatedFrameEvent> event = it->second;

  EXPECT_EQ(rtp_timestamp1, event->rtp_timestamp());

  ASSERT_EQ(2, event->event_type_size());
  EXPECT_EQ(media::cast::proto::AUDIO_PLAYOUT_DELAY, event->event_type(0));
  EXPECT_EQ(media::cast::proto::AUDIO_FRAME_DECODED, event->event_type(1));

  ASSERT_EQ(2, event->event_timestamp_micros_size());
  EXPECT_EQ(now1.ToInternalValue(), event->event_timestamp_micros(0));
  EXPECT_EQ(now3.ToInternalValue(), event->event_timestamp_micros(1));

  it = frame_events.find(200);
  ASSERT_TRUE(it != frame_events.end());

  event = it->second;

  EXPECT_EQ(rtp_timestamp2, event->rtp_timestamp());

  ASSERT_EQ(1, event->event_type_size());
  EXPECT_EQ(media::cast::proto::AUDIO_FRAME_ENCODED, event->event_type(0));

  ASSERT_EQ(1, event->event_timestamp_micros_size());
  EXPECT_EQ(now2.ToInternalValue(), event->event_timestamp_micros(0));
}

TEST_F(EncodingEventSubscriberTest, PacketEvent) {
  Init(AUDIO_EVENT);
  base::TimeTicks now(testing_clock_->NowTicks());
  RtpTimestamp rtp_timestamp = 100;
  int packet_id = 2;
  int size = 100;
  cast_environment_->Logging()->InsertPacketEvent(
      now, kAudioPacketReceived, rtp_timestamp, /*frame_id*/ 0, packet_id,
      /*max_packet_id*/ 10, size);

  PacketEventMap packet_events;
  event_subscriber_->GetPacketEventsAndReset(&packet_events);

  ASSERT_EQ(1u, packet_events.size());

  PacketEventMap::iterator it = packet_events.find(rtp_timestamp);
  ASSERT_TRUE(it != packet_events.end());

  linked_ptr<AggregatedPacketEvent> event = it->second;

  EXPECT_EQ(rtp_timestamp, event->rtp_timestamp());

  ASSERT_EQ(1, event->base_packet_event_size());
  const BasePacketEvent& base_event = event->base_packet_event(0);
  EXPECT_EQ(packet_id, base_event.packet_id());
  ASSERT_EQ(1, base_event.event_type_size());
  EXPECT_EQ(media::cast::proto::AUDIO_PACKET_RECEIVED,
            base_event.event_type(0));
  ASSERT_EQ(1, base_event.event_timestamp_micros_size());
  EXPECT_EQ(now.ToInternalValue(), base_event.event_timestamp_micros(0));

  event_subscriber_->GetPacketEventsAndReset(&packet_events);
  EXPECT_TRUE(packet_events.empty());
}

TEST_F(EncodingEventSubscriberTest, MultiplePacketEventsForPacket) {
  Init(OTHER_EVENT);
  base::TimeTicks now1(testing_clock_->NowTicks());
  RtpTimestamp rtp_timestamp = 100;
  int packet_id = 2;
  int size = 100;
  cast_environment_->Logging()->InsertPacketEvent(
      now1, kPacketSentToPacer, rtp_timestamp, /*frame_id*/ 0, packet_id,
      /*max_packet_id*/ 10, size);

  testing_clock_->Advance(base::TimeDelta::FromMilliseconds(20));
  base::TimeTicks now2(testing_clock_->NowTicks());
  cast_environment_->Logging()->InsertPacketEvent(
      now2, kPacketSentToNetwork, rtp_timestamp, /*frame_id*/ 0, packet_id,
      /*max_packet_id*/ 10, size);

  PacketEventMap packet_events;
  event_subscriber_->GetPacketEventsAndReset(&packet_events);

  ASSERT_EQ(1u, packet_events.size());

  PacketEventMap::iterator it = packet_events.find(rtp_timestamp);
  ASSERT_TRUE(it != packet_events.end());

  linked_ptr<AggregatedPacketEvent> event = it->second;

  EXPECT_EQ(rtp_timestamp, event->rtp_timestamp());

  ASSERT_EQ(1, event->base_packet_event_size());
  const BasePacketEvent& base_event = event->base_packet_event(0);
  EXPECT_EQ(packet_id, base_event.packet_id());
  ASSERT_EQ(2, base_event.event_type_size());
  EXPECT_EQ(media::cast::proto::PACKET_SENT_TO_PACER, base_event.event_type(0));
  EXPECT_EQ(media::cast::proto::PACKET_SENT_TO_NETWORK,
            base_event.event_type(1));
  ASSERT_EQ(2, base_event.event_timestamp_micros_size());
  EXPECT_EQ(now1.ToInternalValue(), base_event.event_timestamp_micros(0));
  EXPECT_EQ(now2.ToInternalValue(), base_event.event_timestamp_micros(1));
}

TEST_F(EncodingEventSubscriberTest, MultiplePacketEventsForFrame) {
  Init(OTHER_EVENT);
  base::TimeTicks now1(testing_clock_->NowTicks());
  RtpTimestamp rtp_timestamp = 100;
  int packet_id_1 = 2;
  int packet_id_2 = 3;
  int size = 100;
  cast_environment_->Logging()->InsertPacketEvent(
      now1, kPacketSentToPacer, rtp_timestamp, /*frame_id*/ 0, packet_id_1,
      /*max_packet_id*/ 10, size);

  testing_clock_->Advance(base::TimeDelta::FromMilliseconds(20));
  base::TimeTicks now2(testing_clock_->NowTicks());
  cast_environment_->Logging()->InsertPacketEvent(
      now2, kPacketRetransmitted, rtp_timestamp, /*frame_id*/ 0, packet_id_2,
      /*max_packet_id*/ 10, size);

  PacketEventMap packet_events;
  event_subscriber_->GetPacketEventsAndReset(&packet_events);

  ASSERT_EQ(1u, packet_events.size());

  PacketEventMap::iterator it = packet_events.find(rtp_timestamp);
  ASSERT_TRUE(it != packet_events.end());

  linked_ptr<AggregatedPacketEvent> event = it->second;

  EXPECT_EQ(rtp_timestamp, event->rtp_timestamp());

  ASSERT_EQ(2, event->base_packet_event_size());
  const BasePacketEvent& base_event = event->base_packet_event(0);
  EXPECT_EQ(packet_id_1, base_event.packet_id());
  ASSERT_EQ(1, base_event.event_type_size());
  EXPECT_EQ(media::cast::proto::PACKET_SENT_TO_PACER, base_event.event_type(0));
  ASSERT_EQ(1, base_event.event_timestamp_micros_size());
  EXPECT_EQ(now1.ToInternalValue(), base_event.event_timestamp_micros(0));

  const BasePacketEvent& base_event_2 = event->base_packet_event(1);
  EXPECT_EQ(packet_id_2, base_event_2.packet_id());
  ASSERT_EQ(1, base_event_2.event_type_size());
  EXPECT_EQ(media::cast::proto::PACKET_RETRANSMITTED,
            base_event_2.event_type(0));
  ASSERT_EQ(1, base_event_2.event_timestamp_micros_size());
  EXPECT_EQ(now2.ToInternalValue(), base_event_2.event_timestamp_micros(0));
}

TEST_F(EncodingEventSubscriberTest, MultiplePacketEvents) {
  Init(OTHER_EVENT);
  base::TimeTicks now1(testing_clock_->NowTicks());
  RtpTimestamp rtp_timestamp_1 = 100;
  RtpTimestamp rtp_timestamp_2 = 200;
  int packet_id_1 = 2;
  int packet_id_2 = 3;
  int size = 100;
  cast_environment_->Logging()->InsertPacketEvent(
      now1, kPacketSentToPacer, rtp_timestamp_1, /*frame_id*/ 0, packet_id_1,
      /*max_packet_id*/ 10, size);

  testing_clock_->Advance(base::TimeDelta::FromMilliseconds(20));
  base::TimeTicks now2(testing_clock_->NowTicks());
  cast_environment_->Logging()->InsertPacketEvent(
      now2, kPacketRetransmitted, rtp_timestamp_2, /*frame_id*/ 0, packet_id_2,
      /*max_packet_id*/ 10, size);

  PacketEventMap packet_events;
  event_subscriber_->GetPacketEventsAndReset(&packet_events);

  ASSERT_EQ(2u, packet_events.size());

  PacketEventMap::iterator it = packet_events.find(rtp_timestamp_1);
  ASSERT_TRUE(it != packet_events.end());

  linked_ptr<AggregatedPacketEvent> event = it->second;

  EXPECT_EQ(rtp_timestamp_1, event->rtp_timestamp());

  ASSERT_EQ(1, event->base_packet_event_size());
  const BasePacketEvent& base_event = event->base_packet_event(0);
  EXPECT_EQ(packet_id_1, base_event.packet_id());
  ASSERT_EQ(1, base_event.event_type_size());
  EXPECT_EQ(media::cast::proto::PACKET_SENT_TO_PACER, base_event.event_type(0));
  ASSERT_EQ(1, base_event.event_timestamp_micros_size());
  EXPECT_EQ(now1.ToInternalValue(), base_event.event_timestamp_micros(0));

  it = packet_events.find(rtp_timestamp_2);
  ASSERT_TRUE(it != packet_events.end());

  event = it->second;

  EXPECT_EQ(rtp_timestamp_2, event->rtp_timestamp());

  ASSERT_EQ(1, event->base_packet_event_size());
  const BasePacketEvent& base_event_2 = event->base_packet_event(0);
  EXPECT_EQ(packet_id_2, base_event_2.packet_id());
  ASSERT_EQ(1, base_event_2.event_type_size());
  EXPECT_EQ(media::cast::proto::PACKET_RETRANSMITTED,
            base_event_2.event_type(0));
  ASSERT_EQ(1, base_event_2.event_timestamp_micros_size());
  EXPECT_EQ(now2.ToInternalValue(), base_event_2.event_timestamp_micros(0));
}

}  // namespace cast
}  // namespace media
