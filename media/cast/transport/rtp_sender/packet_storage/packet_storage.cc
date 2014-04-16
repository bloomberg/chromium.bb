// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/transport/rtp_sender/packet_storage/packet_storage.h"

#include <string>

#include "base/logging.h"

namespace media {
namespace cast {
namespace transport {

// Limit the max time delay to avoid frame id wrap around; 256 / 60 fps.
const int kMaxAllowedTimeStoredMs = 4000;

typedef PacketMap::iterator PacketMapIterator;
typedef TimeToPacketMap::iterator TimeToPacketIterator;


PacketStorage::PacketStorage(base::TickClock* clock, int max_time_stored_ms)
    : clock_(clock) {
  max_time_stored_ = base::TimeDelta::FromMilliseconds(max_time_stored_ms);
  DCHECK_LE(max_time_stored_ms, kMaxAllowedTimeStoredMs) << "Invalid argument";
}

PacketStorage::~PacketStorage() {
}

void PacketStorage::CleanupOldPackets(base::TimeTicks now) {
  TimeToPacketIterator time_it = time_to_packet_map_.begin();

  // Check max size.
  while (time_to_packet_map_.size() >= kMaxStoredPackets) {
    PacketMapIterator store_it = stored_packets_.find(time_it->second);

    // We should always find the packet.
    DCHECK(store_it != stored_packets_.end()) << "Invalid state";
    time_to_packet_map_.erase(time_it);
    // Save the pointer.
    PacketRef storted_packet = store_it->second;
    stored_packets_.erase(store_it);
    time_it = time_to_packet_map_.begin();
  }

  // Time out old packets.
  while (time_it != time_to_packet_map_.end()) {
    if (now < time_it->first + max_time_stored_) {
      break;
    }
    // Packet too old.
    PacketMapIterator store_it = stored_packets_.find(time_it->second);

    // We should always find the packet.
    DCHECK(store_it != stored_packets_.end()) << "Invalid state";
    time_to_packet_map_.erase(time_it);
    // Save the pointer.
    PacketRef storted_packet = store_it->second;
    stored_packets_.erase(store_it);
    time_it = time_to_packet_map_.begin();
  }
}

void PacketStorage::StorePacket(uint32 frame_id,
                                uint16 packet_id,
                                PacketRef packet) {
  base::TimeTicks now = clock_->NowTicks();
  CleanupOldPackets(now);

  // Internally we only use the 8 LSB of the frame id.
  uint32 index = ((0xff & frame_id) << 16) + packet_id;
  PacketMapIterator it = stored_packets_.find(index);
  if (it != stored_packets_.end()) {
    // We have already saved this.
    DCHECK(false) << "Invalid state";
    return;
  }
  stored_packets_[index] = packet;
  time_to_packet_map_.insert(std::make_pair(now, index));
}

void PacketStorage::GetPackets(
    const MissingFramesAndPacketsMap& missing_frames_and_packets,
    PacketList* packets_to_resend) {

  // Iterate over all frames in the list.
  for (MissingFramesAndPacketsMap::const_iterator it =
           missing_frames_and_packets.begin();
       it != missing_frames_and_packets.end();
       ++it) {
    uint8 frame_id = it->first;
    const PacketIdSet& packets_set = it->second;
    bool success = false;

    if (packets_set.empty()) {
      VLOG(1) << "Missing all packets in frame " << static_cast<int>(frame_id);
      uint16 packet_id = 0;
      do {
        // Get packet from storage.
        success = GetPacket(frame_id, packet_id, packets_to_resend);
        ++packet_id;
      } while (success);
    } else {
      // Iterate over all of the packets in the frame.
      for (PacketIdSet::const_iterator set_it = packets_set.begin();
           set_it != packets_set.end();
           ++set_it) {
        GetPacket(frame_id, *set_it, packets_to_resend);
      }
    }
  }
}

bool PacketStorage::GetPacket(uint8 frame_id,
                              uint16 packet_id,
                              PacketList* packets) {
  // Internally we only use the 8 LSB of the frame id.
  uint32 index = (static_cast<uint32>(frame_id) << 16) + packet_id;
  PacketMapIterator it = stored_packets_.find(index);
  if (it == stored_packets_.end()) {
    return false;
  }
  // Minor trickery, the caller (rtp_sender.cc) really wants a copy of the
  // packet so that it can update the sequence number before it sends it to
  // the transport. If the packet only has one ref, we can safely let
  // rtp_sender.cc have our packet and modify it. If it has more references
  // then we must return a copy of it instead. This should really only happen
  // when rtp_sender.cc is trying to re-send a packet that is already in the
  // queue to sent.
  if (it->second->HasOneRef()) {
    packets->push_back(it->second);
  } else {
    packets->push_back(make_scoped_refptr(
        new base::RefCountedData<Packet>(it->second->data)));
  }
  return true;
}

}  // namespace transport
}  // namespace cast
}  // namespace media
