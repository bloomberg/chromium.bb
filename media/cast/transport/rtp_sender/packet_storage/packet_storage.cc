// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/transport/rtp_sender/packet_storage/packet_storage.h"

#include <string>

#include "base/logging.h"

namespace media {
namespace cast {
namespace transport {

typedef PacketMap::iterator PacketMapIterator;

PacketStorage::PacketStorage(int stored_frames)
    : stored_frames_(stored_frames) {
}

PacketStorage::~PacketStorage() {
}

bool PacketStorage::IsValid() const {
  return stored_frames_ > 0 && stored_frames_ <= kMaxStoredFrames;
}

void PacketStorage::CleanupOldPackets(uint32 current_frame_id) {
  uint32 frame_to_remove = current_frame_id - stored_frames_;
  while (!stored_packets_.empty()) {
    if (IsOlderFrameId(stored_packets_.begin()->first.first,
                       frame_to_remove)) {
      stored_packets_.erase(stored_packets_.begin());
    } else {
      break;
    }
  }
}

void PacketStorage::StorePacket(uint32 frame_id,
                                uint16 packet_id,
                                const PacketKey& key,
                                PacketRef packet) {
  CleanupOldPackets(frame_id);
  StorageIndex index(frame_id, packet_id);
  PacketMapIterator it = stored_packets_.find(index);
  if (it != stored_packets_.end()) {
    // We have already saved this.
    DCHECK(false) << "Invalid state";
    return;
  }
  stored_packets_[index] = std::make_pair(key, packet);
}

void PacketStorage::GetPackets(
    const MissingFramesAndPacketsMap& missing_frames_and_packets,
    SendPacketVector* packets_to_resend) {

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

bool PacketStorage::GetPacket32(uint32 frame_id,
                                uint16 packet_id,
                                SendPacketVector* packets) {
  StorageIndex index(frame_id, packet_id);
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
  if (it->second.second->HasOneRef()) {
    packets->push_back(it->second);
  } else {
    packets->push_back(
        std::make_pair(it->second.first,
                       make_scoped_refptr(
                           new base::RefCountedData<Packet>(
                               it->second.second->data))));
  }
  return true;
}

bool PacketStorage::GetPacket(uint8 frame_id_8bit,
                              uint16 packet_id,
                              SendPacketVector* packets) {
  if (stored_packets_.empty()) {
    return false;
  }
  uint32 last_stored = stored_packets_.rbegin()->first.first;
  uint32 frame_id_32bit = (last_stored & ~0xFF) | frame_id_8bit;
  if (IsNewerFrameId(frame_id_32bit, last_stored)) {
    frame_id_32bit -= 0x100;
  }
  DCHECK_EQ(frame_id_8bit, frame_id_32bit & 0xff);
  DCHECK(IsOlderFrameId(frame_id_32bit, last_stored) &&
         IsNewerFrameId(frame_id_32bit + stored_frames_ + 1, last_stored))
      << " 32bit: " << frame_id_32bit
      << " 8bit: " << static_cast<int>(frame_id_8bit)
      << " last_stored: " << last_stored;
  return GetPacket32(frame_id_32bit, packet_id, packets);
}

}  // namespace transport
}  // namespace cast
}  // namespace media
