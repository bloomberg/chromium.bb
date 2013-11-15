// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/logging_internal.h"

namespace media {
namespace cast {

FrameLogData::FrameLogData(base::TickClock* clock)
    : clock_(clock),
      frame_map_() {}

FrameLogData::~FrameLogData() {}

void FrameLogData::Insert(uint32 rtp_timestamp, uint32 frame_id) {
  FrameEvent info;
  InsertBase(rtp_timestamp, frame_id, info);
}

void FrameLogData::InsertWithSize(
      uint32 rtp_timestamp, uint32 frame_id, int size) {
  FrameEvent info;
  info.size = size;
  InsertBase(rtp_timestamp, frame_id, info);
}

void FrameLogData::InsertWithDelay(
    uint32 rtp_timestamp, uint32 frame_id, base::TimeDelta delay) {
  FrameEvent info;
  info.delay_delta = delay;
  InsertBase(rtp_timestamp, frame_id, info);
}

void FrameLogData::InsertBase(
    uint32 rtp_timestamp, uint32 frame_id, FrameEvent info) {
  info.timestamp = clock_->NowTicks();
  info.frame_id = frame_id;
  frame_map_.insert(std::make_pair(rtp_timestamp, info));
}

PacketLogData::PacketLogData(base::TickClock* clock)
    : clock_(clock),
      packet_map_() {}

PacketLogData::~PacketLogData() {}

void PacketLogData::Insert(uint32 rtp_timestamp,
    uint32 frame_id, uint16 packet_id, uint16 max_packet_id, int size) {
  PacketEvent info;
  info.size = size;
  info.max_packet_id = max_packet_id;
  info.frame_id = frame_id;
  info.timestamp = clock_->NowTicks();
  // Is this a new frame?
  PacketMap::iterator it = packet_map_.find(rtp_timestamp);
  if (it == packet_map_.end()) {
    // New rtp_timestamp id - create base packet map.
    BasePacketMap base_map;
    base_map.insert(std::make_pair(packet_id, info));
    packet_map_.insert(std::make_pair(rtp_timestamp, base_map));
  } else {
    // Existing rtp_timestamp.
    it->second.insert(std::make_pair(packet_id, info));
  }
}

GenericLogData::GenericLogData(base::TickClock* clock)
    : clock_(clock) {}

GenericLogData::~GenericLogData() {}

void GenericLogData::Insert(int data) {
  data_.push_back(data);
  timestamp_.push_back(clock_->NowTicks());
}

}  // namespace cast
}  // namespace media
