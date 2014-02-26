// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/log_serializer.h"

#include "base/big_endian.h"

namespace media {
namespace cast {

LogSerializer::LogSerializer(const int max_serialized_bytes)
    : index_so_far_(0), max_serialized_bytes_(max_serialized_bytes) {
  DCHECK_GT(max_serialized_bytes_, 0);
}

LogSerializer::~LogSerializer() {}

// The format is as follows:
//   32-bit integer describing |stream_id|.
//   32-bit integer describing |first_rtp_timestamp|.
//   32-bit integer describing number of frame events.
//   (The following repeated for number of frame events):
//     16-bit integer describing the following AggregatedFrameEvent proto size
//         in bytes.
//     The AggregatedFrameEvent proto.
//   32-bit integer describing number of packet events.
//   (The following repeated for number of packet events):
//     16-bit integer describing the following AggregatedPacketEvent proto
//         size in bytes.
//     The AggregatedPacketEvent proto.
bool LogSerializer::SerializeEventsForStream(
    const int stream_id,
    const FrameEventMap& frame_events,
    const PacketEventMap& packet_events,
    const RtpTimestamp first_rtp_timestamp) {
  if (!serialized_log_so_far_) {
    serialized_log_so_far_.reset(new std::string(max_serialized_bytes_, 0));
  }

  int remaining_space = serialized_log_so_far_->size() - index_so_far_;
  if (remaining_space <= 0)
    return false;

  base::BigEndianWriter writer(&(*serialized_log_so_far_)[index_so_far_],
                              remaining_space);

  // Write stream ID.
  if (!writer.WriteU32(stream_id))
    return false;

  // Write first RTP timestamp.
  if (!writer.WriteU32(first_rtp_timestamp))
    return false;

  // Frame events - write size first, then write entries.
  if (!writer.WriteU32(frame_events.size()))
    return false;

  for (media::cast::FrameEventMap::const_iterator it = frame_events.begin();
       it != frame_events.end();
       ++it) {
    int proto_size = it->second->ByteSize();

    // Write size of the proto, then write the proto.
    if (!writer.WriteU16(proto_size))
      return false;
    if (!it->second->SerializeToArray(writer.ptr(), writer.remaining()))
      return false;
    if (!writer.Skip(proto_size))
      return false;
  }

  // Write packet events.
  if (!writer.WriteU32(packet_events.size()))
    return false;
  for (media::cast::PacketEventMap::const_iterator it = packet_events.begin();
       it != packet_events.end();
       ++it) {
    int proto_size = it->second->ByteSize();
    // Write size of the proto, then write the proto.
    if (!writer.WriteU16(proto_size))
      return false;
    if (!it->second->SerializeToArray(writer.ptr(), writer.remaining()))
      return false;
    if (!writer.Skip(proto_size))
      return false;
  }

  index_so_far_ = serialized_log_so_far_->size() - writer.remaining();
  return true;
}

scoped_ptr<std::string> LogSerializer::GetSerializedLogAndReset() {
  serialized_log_so_far_->resize(index_so_far_);
  index_so_far_ = 0;
  return serialized_log_so_far_.Pass();
}

int LogSerializer::GetSerializedLength() const { return index_so_far_; }

}  // namespace cast
}  // namespace media
