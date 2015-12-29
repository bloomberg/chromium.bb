// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_CAST_DEFINES_H_
#define MEDIA_CAST_CAST_DEFINES_H_

#include <stdint.h>

#include "base/time/time.h"

namespace media {
namespace cast {

// TODO(miu): All remaining functions in this file are to be replaced with
// methods in well-defined data types.  http://crbug.com/530839

inline bool IsNewerFrameId(uint32_t frame_id, uint32_t prev_frame_id) {
  return (frame_id != prev_frame_id) &&
         static_cast<uint32_t>(frame_id - prev_frame_id) < 0x80000000;
}

inline bool IsOlderFrameId(uint32_t frame_id, uint32_t prev_frame_id) {
  return (frame_id == prev_frame_id) || IsNewerFrameId(prev_frame_id, frame_id);
}

inline bool IsNewerPacketId(uint16_t packet_id, uint16_t prev_packet_id) {
  return (packet_id != prev_packet_id) &&
         static_cast<uint16_t>(packet_id - prev_packet_id) < 0x8000;
}

inline bool IsNewerSequenceNumber(uint16_t sequence_number,
                                  uint16_t prev_sequence_number) {
  // Same function as IsNewerPacketId just different data and name.
  return IsNewerPacketId(sequence_number, prev_sequence_number);
}

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_CAST_DEFINES_H_
