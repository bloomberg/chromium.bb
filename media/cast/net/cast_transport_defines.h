// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_NET_CAST_TRANSPORT_DEFINES_H_
#define MEDIA_CAST_NET_CAST_TRANSPORT_DEFINES_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"

namespace media {
namespace cast {

// Enums used to indicate transport readiness state.
enum CastTransportStatus {
  TRANSPORT_AUDIO_UNINITIALIZED = 0,
  TRANSPORT_VIDEO_UNINITIALIZED,
  TRANSPORT_AUDIO_INITIALIZED,
  TRANSPORT_VIDEO_INITIALIZED,
  TRANSPORT_INVALID_CRYPTO_CONFIG,
  TRANSPORT_SOCKET_ERROR,
  CAST_TRANSPORT_STATUS_LAST = TRANSPORT_SOCKET_ERROR
};

// kRtcpCastAllPacketsLost is used in PacketIDSet and
// on the wire to mean that ALL packets for a particular
// frame are lost.
const uint16_t kRtcpCastAllPacketsLost = 0xffff;

// kRtcpCastLastPacket is used in PacketIDSet to ask for
// the last packet of a frame to be retransmitted.
const uint16_t kRtcpCastLastPacket = 0xfffe;

const size_t kMaxIpPacketSize = 1500;

// Each uint16_t represents one packet id within a cast frame.
// Can also contain kRtcpCastAllPacketsLost and kRtcpCastLastPacket.
using PacketIdSet = std::set<uint16_t>;
// Each uint8_t represents one cast frame.
using MissingFramesAndPacketsMap = std::map<uint8_t, PacketIdSet>;

using Packet = std::vector<uint8_t>;
using PacketRef = scoped_refptr<base::RefCountedData<Packet>>;
using PacketList = std::vector<PacketRef>;

class FrameIdWrapHelperTest;

// TODO(miu): UGLY IN-LINE DEFINITION IN HEADER FILE!  Move to appropriate
// location, separated into .h and .cc files.  http://crbug.com/530839
class FrameIdWrapHelper {
 public:
  explicit FrameIdWrapHelper(uint32_t start_frame_id)
      : largest_frame_id_seen_(start_frame_id) {}

  uint32_t MapTo32bitsFrameId(const uint8_t over_the_wire_frame_id) {
    uint32_t ret = (largest_frame_id_seen_ & ~0xff) | over_the_wire_frame_id;
    // Add 1000 to both sides to avoid underflows.
    if (1000 + ret - largest_frame_id_seen_ > 1000 + 127) {
      ret -= 0x100;
    } else if (1000 + ret - largest_frame_id_seen_ < 1000 - 128) {
      ret += 0x100;
    }
    if (1000 + ret - largest_frame_id_seen_ > 1000) {
      largest_frame_id_seen_ = ret;
    }
    return ret;
  }

 private:
  friend class FrameIdWrapHelperTest;

  uint32_t largest_frame_id_seen_;

  DISALLOW_COPY_AND_ASSIGN(FrameIdWrapHelper);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_NET_CAST_TRANSPORT_DEFINES_H_
