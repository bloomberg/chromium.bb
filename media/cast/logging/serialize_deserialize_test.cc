// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Joint LogSerializer and LogDeserializer testing to make sure they stay in
// sync.

#include "media/cast/logging/log_deserializer.h"
#include "media/cast/logging/log_serializer.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/proto/proto_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::cast::proto::AggregatedFrameEvent;
using media::cast::proto::AggregatedPacketEvent;
using media::cast::proto::BasePacketEvent;

namespace {

const media::cast::CastLoggingEvent kVideoFrameEvents[] = {
    media::cast::kVideoFrameCaptured,      media::cast::kVideoFrameReceived,
    media::cast::kVideoFrameSentToEncoder, media::cast::kVideoFrameEncoded,
    media::cast::kVideoFrameDecoded,       media::cast::kVideoRenderDelay};

const media::cast::CastLoggingEvent kVideoPacketEvents[] = {
    media::cast::kPacketSentToPacer, media::cast::kPacketSentToNetwork,
    media::cast::kVideoPacketReceived};

// The frame event fields cycle through these numbers.
const int kEncodedFrameSize[] = {512, 425, 399, 400, 237};
const int kDelayMillis[] = {15, 4, 8, 42, 23, 16};
}

namespace media {
namespace cast {

class SerializeDeserializeTest : public ::testing::Test {
 protected:
  SerializeDeserializeTest() : serializer_(kMaxSerializedLogBytes) {}

  virtual ~SerializeDeserializeTest() {}

  LogSerializer serializer_;
};

TEST_F(SerializeDeserializeTest, Basic) {
  RtpTimestamp first_rtp_timestamp = 12345678 * 90;
  FrameEventMap frame_event_map;
  PacketEventMap packet_event_map;

  int64 event_time_micros = 0;
  // Insert frame and packet events with RTP timestamps 0, 90, 180, ...
  for (int i = 0; i < 10; i++) {
    linked_ptr<AggregatedFrameEvent> frame_event(new AggregatedFrameEvent);
    frame_event->set_rtp_timestamp(i * 90);
    for (uint32 event_index = 0; event_index < arraysize(kVideoFrameEvents);
         ++event_index) {
      frame_event->add_event_type(
          ToProtoEventType(kVideoFrameEvents[event_index]));
      frame_event->add_event_timestamp_micros(event_time_micros);
      event_time_micros += 1024;
    }
    frame_event->set_encoded_frame_size(
        kEncodedFrameSize[i % arraysize(kEncodedFrameSize)]);
    frame_event->set_delay_millis(kDelayMillis[i % arraysize(kDelayMillis)]);

    frame_event_map.insert(
        std::make_pair(frame_event->rtp_timestamp(), frame_event));
  }

  event_time_micros = 0;
  int packet_id = 0;
  for (int i = 0; i < 10; i++) {
    linked_ptr<AggregatedPacketEvent> packet_event(new AggregatedPacketEvent);
    packet_event->set_rtp_timestamp(i * 90);
    for (int j = 0; j < 10; j++) {
      BasePacketEvent* base_event = packet_event->add_base_packet_event();
      base_event->set_packet_id(packet_id);
      packet_id++;
      for (uint32 event_index = 0; event_index < arraysize(kVideoPacketEvents);
           ++event_index) {
        base_event->add_event_type(
            ToProtoEventType(kVideoPacketEvents[event_index]));
        base_event->add_event_timestamp_micros(event_time_micros);
        event_time_micros += 256;
      }
    }
    packet_event_map.insert(
        std::make_pair(packet_event->rtp_timestamp(), packet_event));
  }

  bool success = serializer_.SerializeEventsForStream(
      false, frame_event_map, packet_event_map, first_rtp_timestamp);
  uint32 length = serializer_.GetSerializedLength();
  scoped_ptr<std::string> serialized = serializer_.GetSerializedLogAndReset();
  ASSERT_TRUE(success);
  ASSERT_EQ(length, serialized->size());

  bool returned_is_audio;
  FrameEventMap returned_frame_events;
  PacketEventMap returned_packet_events;
  RtpTimestamp returned_first_rtp_timestamp;
  success = DeserializeEvents(*serialized,
                              &returned_is_audio,
                              &returned_frame_events,
                              &returned_packet_events,
                              &returned_first_rtp_timestamp);
  ASSERT_TRUE(success);
  EXPECT_FALSE(returned_is_audio);
  EXPECT_EQ(first_rtp_timestamp, returned_first_rtp_timestamp);

  // Check that the returned map is equal to the original map.
  EXPECT_EQ(frame_event_map.size(), returned_frame_events.size());
  for (FrameEventMap::iterator frame_it = returned_frame_events.begin();
       frame_it != returned_frame_events.end();
       ++frame_it) {
    FrameEventMap::iterator original_it = frame_event_map.find(frame_it->first);
    ASSERT_NE(frame_event_map.end(), original_it);
    // Compare protos by serializing and checking the bytes.
    EXPECT_EQ(original_it->second->SerializeAsString(),
              frame_it->second->SerializeAsString());
    frame_event_map.erase(original_it);
  }
  EXPECT_TRUE(frame_event_map.empty());

  EXPECT_EQ(packet_event_map.size(), returned_packet_events.size());
  for (PacketEventMap::iterator packet_it = returned_packet_events.begin();
       packet_it != returned_packet_events.end();
       ++packet_it) {
    PacketEventMap::iterator original_it =
        packet_event_map.find(packet_it->first);
    ASSERT_NE(packet_event_map.end(), original_it);
    // Compare protos by serializing and checking the bytes.
    EXPECT_EQ(original_it->second->SerializeAsString(),
              packet_it->second->SerializeAsString());
    packet_event_map.erase(original_it);
  }
  EXPECT_TRUE(packet_event_map.empty());
}

}  // namespace cast
}  // namespace media
