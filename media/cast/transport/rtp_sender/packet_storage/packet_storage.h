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

namespace media {
namespace cast {
namespace transport {

class StoredPacket;
typedef std::map<uint32, PacketRef> PacketMap;
typedef std::multimap<base::TimeTicks, uint32> TimeToPacketMap;

class PacketStorage {
 public:
  static const unsigned int kMaxStoredPackets = 1000;

  PacketStorage(base::TickClock* clock, int max_time_stored_ms);
  virtual ~PacketStorage();

  void StorePacket(uint32 frame_id, uint16 packet_id, PacketRef packet);

  // Copies all missing packets into the packet list.
  void GetPackets(
      const MissingFramesAndPacketsMap& missing_frames_and_packets,
      PacketList* packets_to_resend);

  // Copies packet into the packet list.
  bool GetPacket(uint8 frame_id, uint16 packet_id, PacketList* packets);

 private:
  void CleanupOldPackets(base::TimeTicks now);

  base::TickClock* const clock_;  // Not owned by this class.
  base::TimeDelta max_time_stored_;
  PacketMap stored_packets_;
  TimeToPacketMap time_to_packet_map_;

  DISALLOW_COPY_AND_ASSIGN(PacketStorage);
};

}  // namespace transport
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TRANSPORT_RTP_SENDER_PACKET_STORAGE_PACKET_STORAGE_H_
