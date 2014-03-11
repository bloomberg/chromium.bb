// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/log_deserializer.h"

#include <map>
#include <utility>

#include "base/big_endian.h"

using media::cast::proto::AggregatedFrameEvent;
using media::cast::proto::AggregatedPacketEvent;

namespace media {
namespace cast {

bool DeserializeEvents(const std::string& data,
                       media::cast::proto::LogMetadata* metadata,
                       FrameEventMap* frame_events,
                       PacketEventMap* packet_events) {
  DCHECK(metadata);
  DCHECK(frame_events);
  DCHECK(packet_events);

  base::BigEndianReader reader(&data[0], data.size());

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

  for (int i = 0; i < num_frame_events; i++) {
    if (!reader.ReadU16(&proto_size))
      return false;

    linked_ptr<AggregatedFrameEvent> frame_event(new AggregatedFrameEvent);
    if (!frame_event->ParseFromArray(reader.ptr(), proto_size))
      return false;
    if (!reader.Skip(proto_size))
      return false;

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
  for (int i = 0; i < num_packet_events; i++) {
    if (!reader.ReadU16(&proto_size))
      return false;

    linked_ptr<AggregatedPacketEvent> packet_event(new AggregatedPacketEvent);
    if (!packet_event->ParseFromArray(reader.ptr(), proto_size))
      return false;
    if (!reader.Skip(proto_size))
      return false;

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

}  // namespace cast
}  // namespace media
