// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_LOGGING_LOGGING_DEFINES_H_
#define MEDIA_CAST_LOGGING_LOGGING_DEFINES_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/time/time.h"

namespace media {
namespace cast {

enum CastLoggingEvent {
  // Generic events.
  kRtt,
  kPacketLoss,
  kJitter,
  kAckReceived,
  kAckSent,
  kLastEvent,
  // Audio sender.
  kAudioFrameCaptured,
  kAudioFrameEncoded,
  // Audio receiver.
  kAudioPlayoutDelay,
  kAudioFrameDecoded,
  // Video sender.
  kVideoFrameCaptured,
  kVideoFrameSentToEncoder,
  kVideoFrameEncoded,
  // Video receiver.
  kVideoFrameDecoded,
  kVideoRenderDelay,
  // Send-side packet events.
  kPacketSentToPacer,
  kPacketSentToNetwork,
  kPacketRetransmited,
  // Receive-side packet events.
  kPacketReceived,
};

std::string CastLoggingToString(CastLoggingEvent event);

struct FrameEvent {
  FrameEvent();
  ~FrameEvent();

  uint32 frame_id;
  int size;  // Encoded size only.
  std::vector<base::TimeTicks> timestamp;
  std::vector<CastLoggingEvent> type;
  base::TimeDelta delay_delta;  // Render/playout delay.
};

// Internal map sorted by packet id.
struct BasePacketInfo {
  BasePacketInfo();
  ~BasePacketInfo();

  int size;
  std::vector<base::TimeTicks> timestamp;
  std::vector<CastLoggingEvent> type;
};

typedef std::map<uint16, BasePacketInfo> BasePacketMap;

struct PacketEvent {
  PacketEvent();
  ~PacketEvent();
  uint32 frame_id;
  int max_packet_id;
  BasePacketMap packet_map;
};

struct GenericEvent {
  GenericEvent();
  ~GenericEvent();
  std::vector<int> value;
  std::vector<base::TimeTicks> timestamp;
};

struct FrameLogStats {
  FrameLogStats();
  ~FrameLogStats();

  double framerate_fps;
  double bitrate_kbps;
  int max_delay_ms;
  int min_delay_ms;
  int avg_delay_ms;
};

// Store all log types in a map based on the event.
typedef std::map<uint32, FrameEvent> FrameRawMap;
typedef std::map<uint32, PacketEvent> PacketRawMap;
typedef std::map<CastLoggingEvent, GenericEvent> GenericRawMap;

typedef std::map<CastLoggingEvent, linked_ptr<FrameLogStats > > FrameStatsMap;
typedef std::map<CastLoggingEvent, double> PacketStatsMap;
typedef std::map<CastLoggingEvent, double> GenericStatsMap;

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_LOGGING_LOGGING_DEFINES_H_
