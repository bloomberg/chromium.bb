// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TRANSPORT_RTP_SENDER_PACKET_STORAGE_PACKET_STORAGE_H_
#define MEDIA_CAST_TRANSPORT_RTP_SENDER_PACKET_STORAGE_PACKET_STORAGE_H_

#include <list>
#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/transport/cast_transport_config.h"
#include "media/cast/transport/cast_transport_defines.h"
#include "media/cast/transport/pacing/paced_sender.h"

namespace media {
namespace cast {
namespace transport {

class StoredPacket;

// StorageIndex contains {frame_id, packet_id}.
typedef std::pair<uint32, uint16> StorageIndex;
typedef std::map<StorageIndex, std::pair<PacketKey, PacketRef> > PacketMap;

// Frame IDs are generally stored as 8-bit values when sent over the
// wire. This means that having a history longer than 255 frames makes
// no sense.
const int kMaxStoredFrames = 255;

class PacketStorage {
 public:
  PacketStorage(int stored_frames);
  virtual ~PacketStorage();

  // Returns true if this class is configured correctly.
  // (stored frames > 0 && stored_frames < kMaxStoredFrames)
  bool IsValid() const;

  void StorePacket(uint32 frame_id,
                   uint16 packet_id,
                   const PacketKey& key,
                   PacketRef packet);

  // Copies all missing packets into the packet list.
  void GetPackets(
      const MissingFramesAndPacketsMap& missing_frames_and_packets,
      SendPacketVector* packets_to_resend);

  // Copies packet into the packet list.
  bool GetPacket(uint8 frame_id_8bit,
                 uint16 packet_id,
                 SendPacketVector* packets);
 private:
  FRIEND_TEST_ALL_PREFIXES(PacketStorageTest, PacketContent);

  // Same as GetPacket, but takes a 32-bit frame id.
  bool GetPacket32(uint32 frame_id,
                   uint16 packet_id,
                   SendPacketVector* packets);
  void CleanupOldPackets(uint32 current_frame_id);

  PacketMap stored_packets_;
  int stored_frames_;

  DISALLOW_COPY_AND_ASSIGN(PacketStorage);
};

}  // namespace transport
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TRANSPORT_RTP_SENDER_PACKET_STORAGE_PACKET_STORAGE_H_
