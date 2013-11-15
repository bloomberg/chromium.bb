// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/logging.h"

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"

namespace media {
namespace cast {

Logging::Logging(base::TickClock* clock)
    : clock_(clock),
      frame_map_(),
      packet_map_(),
      generic_map_(),
      weak_factory_(this) {}

Logging::~Logging() {}

void Logging::InsertFrameEvent(CastLoggingEvent event,
                               uint32 rtp_timestamp,
                               uint32 frame_id) {
  // Is this a new event?
  FrameLogMap::iterator it = frame_map_.find(event);
  if (it == frame_map_.end()) {
    // Create new entry.
    FrameLogData data(clock_);
    data.Insert(rtp_timestamp, frame_id);
    frame_map_.insert(std::make_pair(event, &data));
  } else {
    // Insert to existing entry.
    it->second->Insert(rtp_timestamp, frame_id);
  }
}

void Logging::InsertFrameEventWithSize(CastLoggingEvent event,
                                       uint32 rtp_timestamp,
                                       uint32 frame_id,
                                       int size) {
  // Is this a new event?
  FrameLogMap::iterator it = frame_map_.find(event);
  if (it == frame_map_.end()) {
    // Create new entry.
    FrameLogData data(clock_);
    data.InsertWithSize(rtp_timestamp, frame_id, size);
    frame_map_.insert(std::make_pair(event, &data));
  } else {
    // Insert to existing entry.
    it->second->InsertWithSize(rtp_timestamp, frame_id, size);
  }
}

void Logging::InsertFrameEventWithDelay(CastLoggingEvent event,
                                        uint32 rtp_timestamp,
                                        uint32 frame_id,
                                        base::TimeDelta delay) {
  // Is this a new event?
  FrameLogMap::iterator it = frame_map_.find(event);
  if (it == frame_map_.end()) {
    // Create new entry.
    FrameLogData data(clock_);
    data.InsertWithDelay(rtp_timestamp, frame_id, delay);
    frame_map_.insert(std::make_pair(event, &data));
  } else {
    // Insert to existing entry.
    it->second->InsertWithDelay(rtp_timestamp, frame_id, delay);
  }
}

void Logging::InsertPacketEvent(CastLoggingEvent event,
                                uint32 rtp_timestamp,
                                uint32 frame_id,
                                uint16 packet_id,
                                uint16 max_packet_id,
                                int size) {
  // Is this a new event?
  PacketLogMap::iterator it = packet_map_.find(event);
  if (it == packet_map_.end()) {
    // Create new entry.
    PacketLogData data(clock_);
    data.Insert(rtp_timestamp, frame_id, packet_id, max_packet_id, size);
    packet_map_.insert(std::make_pair(event, &data));
  } else {
    // Insert to existing entry.
    it->second->Insert(rtp_timestamp, frame_id, packet_id, max_packet_id, size);
  }
}

void Logging::InsertGenericEvent(CastLoggingEvent event, int value) {
  // Is this a new event?
  GenericLogMap::iterator it = generic_map_.find(event);
  if (it == generic_map_.end()) {
    // Create new entry.
    GenericLogData data(clock_);
    data.Insert(value);
    generic_map_.insert(std::make_pair(event, &data));
  } else {
    // Insert to existing entry.
    it->second->Insert(value);
  }
}

void Logging::Reset() {
  frame_map_.clear();
  packet_map_.clear();
  generic_map_.clear();
}
}  // namespace cast
}  // namespace media

