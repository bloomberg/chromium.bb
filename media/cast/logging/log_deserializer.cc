// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/log_deserializer.h"

#include <map>
#include <utility>

#include "base/big_endian.h"
#include "third_party/zlib/zlib.h"

using media::cast::FrameEventMap;
using media::cast::PacketEventMap;
using media::cast::RtpTimestamp;
using media::cast::proto::AggregatedFrameEvent;
using media::cast::proto::AggregatedPacketEvent;
using media::cast::proto::LogMetadata;

namespace {

// Use 30MB of temp buffer to hold uncompressed data if |compress| is true.
const int kMaxUncompressedBytes = 30 * 1000 * 1000;

bool DoDeserializeEvents(char* data,
                         int data_bytes,
                         LogMetadata* metadata,
                         FrameEventMap* frame_events,
                         PacketEventMap* packet_events) {
  base::BigEndianReader reader(data, data_bytes);

  uint16 proto_size;
  if (!reader.ReadU16(&proto_size))
    return false;
  if (!metadata->ParseFromArray(reader.ptr(), proto_size))
    return false;
  if (!reader.Skip(proto_size))
    return false;
  FrameEventMap frame_event_map;
  PacketEventMap packet_event_map;

  int num_frame_events = metadata->num_frame_events();
  RtpTimestamp relative_rtp_timestamp = 0;
  for (int i = 0; i < num_frame_events; i++) {
    if (!reader.ReadU16(&proto_size))
      return false;

    linked_ptr<AggregatedFrameEvent> frame_event(new AggregatedFrameEvent);
    if (!frame_event->ParseFromArray(reader.ptr(), proto_size))
      return false;
    if (!reader.Skip(proto_size))
      return false;

    // During serialization the RTP timestamp in proto is relative to previous
    // frame.
    // Adjust RTP timestamp back to value relative to first RTP timestamp.
    frame_event->set_relative_rtp_timestamp(
        frame_event->relative_rtp_timestamp() + relative_rtp_timestamp);
    relative_rtp_timestamp = frame_event->relative_rtp_timestamp();

    std::pair<FrameEventMap::iterator, bool> result = frame_event_map.insert(
        std::make_pair(frame_event->relative_rtp_timestamp(), frame_event));
    if (!result.second) {
      VLOG(1) << "Duplicate frame event entry detected: "
              << frame_event->relative_rtp_timestamp();
      return false;
    }
  }

  frame_events->swap(frame_event_map);

  int num_packet_events = metadata->num_packet_events();
  relative_rtp_timestamp = 0;
  for (int i = 0; i < num_packet_events; i++) {
    if (!reader.ReadU16(&proto_size))
      return false;

    linked_ptr<AggregatedPacketEvent> packet_event(new AggregatedPacketEvent);
    if (!packet_event->ParseFromArray(reader.ptr(), proto_size))
      return false;
    if (!reader.Skip(proto_size))
      return false;

    packet_event->set_relative_rtp_timestamp(
        packet_event->relative_rtp_timestamp() + relative_rtp_timestamp);
    relative_rtp_timestamp = packet_event->relative_rtp_timestamp();

    std::pair<PacketEventMap::iterator, bool> result = packet_event_map.insert(
        std::make_pair(packet_event->relative_rtp_timestamp(), packet_event));
    if (!result.second) {
      VLOG(1) << "Duplicate packet event entry detected: "
              << packet_event->relative_rtp_timestamp();
      return false;
    }
  }

  packet_events->swap(packet_event_map);

  return true;
}

bool Uncompress(char* data,
                int data_bytes,
                int max_uncompressed_bytes,
                char* uncompressed,
                int* uncompressed_bytes) {
  z_stream stream = {0};
  // 16 is added to read in gzip format.
  int result = inflateInit2(&stream, MAX_WBITS + 16);
  DCHECK_EQ(Z_OK, result);

  stream.next_in = reinterpret_cast<uint8*>(data);
  stream.avail_in = data_bytes;
  stream.next_out = reinterpret_cast<uint8*>(uncompressed);
  stream.avail_out = max_uncompressed_bytes;

  result = inflate(&stream, Z_FINISH);
  bool success = (result == Z_STREAM_END);
  if (!success)
    DVLOG(2) << "inflate() failed. Result: " << result;

  result = inflateEnd(&stream);
  DCHECK(result == Z_OK);

  if (success)
    *uncompressed_bytes = max_uncompressed_bytes - stream.avail_out;

  return success;
}
}  // namespace

namespace media {
namespace cast {

bool DeserializeEvents(char* data,
                       int data_bytes,
                       bool compressed,
                       LogMetadata* log_metadata,
                       FrameEventMap* frame_events,
                       PacketEventMap* packet_events) {
  DCHECK(data);
  DCHECK_GT(data_bytes, 0);
  DCHECK(log_metadata);
  DCHECK(frame_events);
  DCHECK(packet_events);

  if (compressed) {
    scoped_ptr<char[]> uncompressed(new char[kMaxUncompressedBytes]);
    int uncompressed_bytes;
    if (!Uncompress(data,
                    data_bytes,
                    kMaxUncompressedBytes,
                    uncompressed.get(),
                    &uncompressed_bytes))
      return false;

    return DoDeserializeEvents(uncompressed.get(),
                               uncompressed_bytes,
                               log_metadata,
                               frame_events,
                               packet_events);
  } else {
    return DoDeserializeEvents(
        data, data_bytes, log_metadata, frame_events, packet_events);
  }
}

}  // namespace cast
}  // namespace media
