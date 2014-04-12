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
using media::cast::proto::LogMetadata;

namespace {

const media::cast::CastLoggingEvent kVideoFrameEvents[] = {
    media::cast::kVideoFrameCaptured,      media::cast::kVideoFrameReceived,
    media::cast::kVideoFrameSentToEncoder, media::cast::kVideoFrameEncoded,
    media::cast::kVideoFrameDecoded,       media::cast::kVideoRenderDelay};

const media::cast::CastLoggingEvent kVideoPacketEvents[] = {
    media::cast::kVideoPacketSentToPacer,
    media::cast::kVideoPacketSentToNetwork, media::cast::kVideoPacketReceived};

// The frame event fields cycle through these numbers.
const int kEncodedFrameSize[] = {512, 425, 399, 400, 237};
const int kDelayMillis[] = {15, 4, 8, 42, 23, 16};

const int kMaxSerializedBytes = 10000;
}

namespace media {
namespace cast {

class SerializeDeserializeTest : public ::testing::Test {
 protected:
  SerializeDeserializeTest()
      : serialized_(new char[kMaxSerializedBytes]), output_bytes_(0) {}

  virtual ~SerializeDeserializeTest() {}

  void Init() {
    metadata_.set_first_rtp_timestamp(12345678 * 90);
    metadata_.set_is_audio(false);
    metadata_.set_num_frame_events(10);
    metadata_.set_num_packet_events(10);

    int64 event_time_ms = 0;
    // Insert frame and packet events with RTP timestamps 0, 90, 180, ...
    for (int i = 0; i < metadata_.num_frame_events(); i++) {
      linked_ptr<AggregatedFrameEvent> frame_event(new AggregatedFrameEvent);
      frame_event->set_relative_rtp_timestamp(i * 90);
      for (uint32 event_index = 0; event_index < arraysize(kVideoFrameEvents);
           ++event_index) {
        frame_event->add_event_type(
            ToProtoEventType(kVideoFrameEvents[event_index]));
        frame_event->add_event_timestamp_ms(event_time_ms);
        event_time_ms += 1024;
      }
      frame_event->set_encoded_frame_size(
          kEncodedFrameSize[i % arraysize(kEncodedFrameSize)]);
      frame_event->set_delay_millis(kDelayMillis[i % arraysize(kDelayMillis)]);

      frame_event_map_.insert(
          std::make_pair(frame_event->relative_rtp_timestamp(), frame_event));
    }

    event_time_ms = 0;
    int packet_id = 0;
    for (int i = 0; i < metadata_.num_packet_events(); i++) {
      linked_ptr<AggregatedPacketEvent> packet_event(new AggregatedPacketEvent);
      packet_event->set_relative_rtp_timestamp(i * 90);
      for (int j = 0; j < 10; j++) {
        BasePacketEvent* base_event = packet_event->add_base_packet_event();
        base_event->set_packet_id(packet_id);
        packet_id++;
        for (uint32 event_index = 0;
             event_index < arraysize(kVideoPacketEvents);
             ++event_index) {
          base_event->add_event_type(
              ToProtoEventType(kVideoPacketEvents[event_index]));
          base_event->add_event_timestamp_ms(event_time_ms);
          event_time_ms += 256;
        }
      }
      packet_event_map_.insert(
          std::make_pair(packet_event->relative_rtp_timestamp(), packet_event));
    }
  }

  void Verify(const LogMetadata& returned_metadata,
              const FrameEventMap& returned_frame_events,
              const PacketEventMap& returned_packet_events) {
    EXPECT_EQ(metadata_.SerializeAsString(),
              returned_metadata.SerializeAsString());

    // Check that the returned map is equal to the original map.
    EXPECT_EQ(frame_event_map_.size(), returned_frame_events.size());
    for (FrameEventMap::const_iterator frame_it = returned_frame_events.begin();
         frame_it != returned_frame_events.end();
         ++frame_it) {
      FrameEventMap::iterator original_it =
          frame_event_map_.find(frame_it->first);
      ASSERT_NE(frame_event_map_.end(), original_it);
      // Compare protos by serializing and checking the bytes.
      EXPECT_EQ(original_it->second->SerializeAsString(),
                frame_it->second->SerializeAsString());
      frame_event_map_.erase(original_it);
    }
    EXPECT_TRUE(frame_event_map_.empty());

    EXPECT_EQ(packet_event_map_.size(), returned_packet_events.size());
    for (PacketEventMap::const_iterator packet_it =
             returned_packet_events.begin();
         packet_it != returned_packet_events.end();
         ++packet_it) {
      PacketEventMap::iterator original_it =
          packet_event_map_.find(packet_it->first);
      ASSERT_NE(packet_event_map_.end(), original_it);
      // Compare protos by serializing and checking the bytes.
      EXPECT_EQ(original_it->second->SerializeAsString(),
                packet_it->second->SerializeAsString());
      packet_event_map_.erase(original_it);
    }
    EXPECT_TRUE(packet_event_map_.empty());
  }

  LogMetadata metadata_;
  FrameEventMap frame_event_map_;
  PacketEventMap packet_event_map_;
  scoped_ptr<char[]> serialized_;
  int output_bytes_;
};

TEST_F(SerializeDeserializeTest, Uncompressed) {
  bool compressed = false;
  Init();

  bool success = SerializeEvents(metadata_,
                                 frame_event_map_,
                                 packet_event_map_,
                                 compressed,
                                 kMaxSerializedBytes,
                                 serialized_.get(),
                                 &output_bytes_);
  ASSERT_TRUE(success);
  ASSERT_GT(output_bytes_, 0);

  FrameEventMap returned_frame_events;
  PacketEventMap returned_packet_events;
  LogMetadata returned_metadata;
  success = DeserializeEvents(serialized_.get(),
                              output_bytes_,
                              compressed,
                              &returned_metadata,
                              &returned_frame_events,
                              &returned_packet_events);
  ASSERT_TRUE(success);

  Verify(returned_metadata, returned_frame_events, returned_packet_events);
}

TEST_F(SerializeDeserializeTest, UncompressedInsufficientSpace) {
  bool compressed = false;
  Init();
  serialized_.reset(new char[100]);
  bool success = SerializeEvents(metadata_,
                                 frame_event_map_,
                                 packet_event_map_,
                                 compressed,
                                 100,
                                 serialized_.get(),
                                 &output_bytes_);
  EXPECT_FALSE(success);
  EXPECT_EQ(0, output_bytes_);
}

TEST_F(SerializeDeserializeTest, Compressed) {
  bool compressed = true;
  Init();
  bool success = SerializeEvents(metadata_,
                                 frame_event_map_,
                                 packet_event_map_,
                                 compressed,
                                 kMaxSerializedBytes,
                                 serialized_.get(),
                                 &output_bytes_);
  ASSERT_TRUE(success);
  ASSERT_GT(output_bytes_, 0);

  FrameEventMap returned_frame_events;
  PacketEventMap returned_packet_events;
  LogMetadata returned_metadata;
  success = DeserializeEvents(serialized_.get(),
                              output_bytes_,
                              compressed,
                              &returned_metadata,
                              &returned_frame_events,
                              &returned_packet_events);
  ASSERT_TRUE(success);
  Verify(returned_metadata, returned_frame_events, returned_packet_events);
}

TEST_F(SerializeDeserializeTest, CompressedInsufficientSpace) {
  bool compressed = true;
  Init();
  serialized_.reset(new char[100]);
  bool success = SerializeEvents(metadata_,
                                 frame_event_map_,
                                 packet_event_map_,
                                 compressed,
                                 100,
                                 serialized_.get(),
                                 &output_bytes_);
  EXPECT_FALSE(success);
  EXPECT_EQ(0, output_bytes_);
}

}  // namespace cast
}  // namespace media
