// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/rtp_sender/packet_storage/packet_storage.h"

#include <string>

#include "base/logging.h"
#include "media/cast/cast_defines.h"

namespace media {
namespace cast {

// Limit the max time delay to avoid frame id wrap around; 256 / 60 fps.
const int kMaxAllowedTimeStoredMs = 4000;

typedef PacketMap::iterator PacketMapIterator;
typedef TimeToPacketMap::iterator TimeToPacketIterator;

class StoredPacket {
 public:
  StoredPacket() {
    packet_.reserve(kIpPacketSize);
  }

  void Save(const Packet* packet) {
    DCHECK_LT(packet->size(), kIpPacketSize) << "Invalid argument";
    packet_.clear();
    packet_.insert(packet_.begin(), packet->begin(), packet->end());
  }

  void GetCopy(PacketList* packets) {
    packets->push_back(Packet(packet_.begin(), packet_.end()));
  }

 private:
  Packet packet_;
};

PacketStorage::PacketStorage(base::TickClock* clock,
                             int max_time_stored_ms)
    : clock_(clock) {
  max_time_stored_ = base::TimeDelta::FromMilliseconds(max_time_stored_ms);
  DCHECK_LE(max_time_stored_ms, kMaxAllowedTimeStoredMs) << "Invalid argument";
}

PacketStorage::~PacketStorage() {
  time_to_packet_map_.clear();

  PacketMapIterator store_it = stored_packets_.begin();
  for (; store_it != stored_packets_.end();
      store_it = stored_packets_.begin()) {
    stored_packets_.erase(store_it);
  }
  while (!free_packets_.empty()) {
    free_packets_.pop_front();
  }
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
    linked_ptr<StoredPacket> storted_packet = store_it->second;
    stored_packets_.erase(store_it);
    // Add this packet to the free list for later re-use.
    free_packets_.push_back(storted_packet);
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
    linked_ptr<StoredPacket> storted_packet = store_it->second;
    stored_packets_.erase(store_it);
    // Add this packet to the free list for later re-use.
    free_packets_.push_back(storted_packet);
    time_it = time_to_packet_map_.begin();
  }
}

void PacketStorage::StorePacket(uint8 frame_id, uint16 packet_id,
                                const Packet* packet) {
  base::TimeTicks now = clock_->NowTicks();
  CleanupOldPackets(now);

  uint32 index = (static_cast<uint32>(frame_id) << 16) + packet_id;
  PacketMapIterator it = stored_packets_.find(index);
  if (it != stored_packets_.end()) {
    // We have already saved this.
    DCHECK(false) << "Invalid state";
    return;
  }
  linked_ptr<StoredPacket> stored_packet;
  if (free_packets_.empty()) {
    // No previous allocated packets allocate one.
    stored_packet.reset(new StoredPacket());
  } else {
    // Re-use previous allocated packet.
    stored_packet = free_packets_.front();
    free_packets_.pop_front();
  }
  stored_packet->Save(packet);
  stored_packets_[index] = stored_packet;
  time_to_packet_map_.insert(std::make_pair(now, index));
}

bool PacketStorage::GetPacket(uint8 frame_id,
                              uint16 packet_id,
                              PacketList* packets) {
  uint32 index = (static_cast<uint32>(frame_id) << 16) + packet_id;
  PacketMapIterator it = stored_packets_.find(index);
  if (it == stored_packets_.end()) {
    return false;
  }
  it->second->GetCopy(packets);
  return true;
}

}  // namespace cast
}  // namespace media
