// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_LOGGING_LOGGING_INTERNAL_H_
#define MEDIA_CAST_LOGGING_LOGGING_INTERNAL_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"

namespace media {
namespace cast {

// TODO(mikhal): Consider storing only the delta time and not absolute time.
struct FrameEvent {
  uint32 frame_id;
  int size;
  base::TimeTicks timestamp;
  base::TimeDelta delay_delta;  // render/playout delay.
};

struct PacketEvent {
  uint32 frame_id;
  int max_packet_id;
  size_t size;
  base::TimeTicks timestamp;
};

// Frame and packet maps are sorted based on the rtp_timestamp.
typedef std::map<uint32, FrameEvent> FrameMap;
typedef std::map<uint16, PacketEvent> BasePacketMap;
typedef std::map<uint32, BasePacketMap> PacketMap;

class FrameLogData {
 public:
  explicit FrameLogData(base::TickClock* clock);
  ~FrameLogData();
  void Insert(uint32 rtp_timestamp, uint32 frame_id);
  // Include size for encoded images (compute bitrate),
  void InsertWithSize(uint32 rtp_timestamp, uint32 frame_id, int size);
  // Include playout/render delay info.
  void InsertWithDelay(
      uint32 rtp_timestamp, uint32 frame_id, base::TimeDelta delay);
  void Reset();

 private:
  void InsertBase(uint32 rtp_timestamp, uint32 frame_id, FrameEvent info);

  base::TickClock* const clock_;  // Not owned by this class.
  FrameMap frame_map_;

  DISALLOW_COPY_AND_ASSIGN(FrameLogData);
};

// TODO(mikhal): Should be able to handle packet bursts.
class PacketLogData {
 public:
  explicit PacketLogData(base::TickClock* clock);
  ~PacketLogData();
  void Insert(uint32 rtp_timestamp, uint32 frame_id, uint16 packet_id,
      uint16 max_packet_id, int size);
  void Reset();

 private:
  base::TickClock* const clock_;  // Not owned by this class.
  PacketMap packet_map_;

  DISALLOW_COPY_AND_ASSIGN(PacketLogData);
};

class GenericLogData {
 public:
  explicit GenericLogData(base::TickClock* clock);
  ~GenericLogData();
  void Insert(int value);
  void Reset();

 private:
  base::TickClock* const clock_;  // Not owned by this class.
  std::vector<int> data_;
  std::vector<base::TimeTicks> timestamp_;

  DISALLOW_COPY_AND_ASSIGN(GenericLogData);
};


}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_LOGGING_LOGGING_INTERNAL_H_
