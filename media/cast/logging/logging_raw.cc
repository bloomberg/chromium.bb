// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/logging_raw.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/time/time.h"

namespace media {
namespace cast {

LoggingRaw::LoggingRaw(base::TickClock* clock)
    : clock_(clock),
      frame_map_(),
      packet_map_(),
      generic_map_(),
      weak_factory_(this) {}

LoggingRaw::~LoggingRaw() {}

void LoggingRaw::InsertFrameEvent(CastLoggingEvent event,
                                  uint32 rtp_timestamp,
                                  uint32 frame_id) {
  InsertBaseFrameEvent(event, frame_id, rtp_timestamp);
}

void LoggingRaw::InsertFrameEventWithSize(CastLoggingEvent event,
                                          uint32 rtp_timestamp,
                                          uint32 frame_id,
                                          int size) {
  InsertBaseFrameEvent(event, frame_id, rtp_timestamp);
  // Now insert size.
  FrameRawMap::iterator it = frame_map_.find(rtp_timestamp);
  DCHECK(it != frame_map_.end());
  it->second.size = size;
}

void LoggingRaw::InsertFrameEventWithDelay(CastLoggingEvent event,
                                           uint32 rtp_timestamp,
                                           uint32 frame_id,
                                           base::TimeDelta delay) {
  InsertBaseFrameEvent(event, frame_id, rtp_timestamp);
  // Now insert delay.
  FrameRawMap::iterator it = frame_map_.find(rtp_timestamp);
  DCHECK(it != frame_map_.end());
  it->second.delay_delta = delay;
}

void LoggingRaw::InsertBaseFrameEvent(CastLoggingEvent event,
                                      uint32 frame_id,
                                      uint32 rtp_timestamp) {
  // Is this a new event?
  FrameRawMap::iterator it = frame_map_.find(rtp_timestamp);
  if (it == frame_map_.end()) {
    // Create a new map entry.
    FrameEvent info;
    info.frame_id = frame_id;
    info.timestamp.push_back(clock_->NowTicks());
    info.type.push_back(event);
    frame_map_.insert(std::make_pair(rtp_timestamp, info));
  } else {
    // Insert to an existing entry.
    it->second.timestamp.push_back(clock_->NowTicks());
    it->second.type.push_back(event);
    // Do we have a valid frame_id?
    // Not all events have a valid frame id.
    if (it->second.frame_id == kFrameIdUnknown && frame_id != kFrameIdUnknown)
      it->second.frame_id = frame_id;
  }
}

void LoggingRaw::InsertPacketEvent(CastLoggingEvent event,
                                   uint32 rtp_timestamp,
                                   uint32 frame_id,
                                   uint16 packet_id,
                                   uint16 max_packet_id,
                                   size_t size) {
  // Is this packet belonging to a new frame?
  PacketRawMap::iterator it = packet_map_.find(rtp_timestamp);
  if (it == packet_map_.end()) {
    // Create a new entry - start with base packet map.
    PacketEvent info;
    info.frame_id = frame_id;
    info.max_packet_id = max_packet_id;
    BasePacketInfo base_info;
    base_info.size = size;
    base_info.timestamp.push_back(clock_->NowTicks());
    base_info.type.push_back(event);
    packet_map_.insert(std::make_pair(rtp_timestamp, info));
  } else {
    // Is this a new packet?
    BasePacketMap::iterator packet_it = it->second.packet_map.find(packet_id);
    if (packet_it == it->second.packet_map.end()) {
      BasePacketInfo base_info;
      base_info.size = size;
      base_info.timestamp.push_back(clock_->NowTicks());
      base_info.type.push_back(event);
      it->second.packet_map.insert(std::make_pair(packet_id, base_info));
    } else {
      packet_it->second.timestamp.push_back(clock_->NowTicks());
      packet_it->second.type.push_back(event);
    }
  }
}

void LoggingRaw::InsertGenericEvent(CastLoggingEvent event, int value) {
  GenericEvent event_data;
  event_data.value.push_back(value);
  event_data.timestamp.push_back(clock_->NowTicks());
  // Is this a new event?
  GenericRawMap::iterator it = generic_map_.find(event);
  if (it == generic_map_.end()) {
    // Create new entry.
    generic_map_.insert(std::make_pair(event, event_data));
  } else {
    // Insert to existing entry.
    it->second.value.push_back(value);
    it->second.timestamp.push_back(clock_->NowTicks());
  }
}

FrameRawMap LoggingRaw::GetFrameData() const {
  return frame_map_;
}

PacketRawMap LoggingRaw::GetPacketData() const {
  return packet_map_;
}

GenericRawMap LoggingRaw::GetGenericData() const {
  return generic_map_;
}

void LoggingRaw::Reset() {
  frame_map_.clear();
  packet_map_.clear();
  generic_map_.clear();
}

}  // namespace cast
}  // namespace media
