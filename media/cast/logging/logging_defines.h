// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_LOGGING_LOGGING_DEFINES_H_
#define MEDIA_CAST_LOGGING_LOGGING_DEFINES_H_

#include <map>
#include <string>
#include <vector>

#include "base/time/time.h"

namespace media {
namespace cast {

static const uint32 kFrameIdUnknown = 0xFFFFFFFF;

struct CastLoggingConfig {
  CastLoggingConfig(bool sender);
  ~CastLoggingConfig();

  bool is_sender;
  bool enable_raw_data_collection;
  bool enable_stats_data_collection;
  bool enable_uma_stats;
  bool enable_tracing;
};

// By default, enable raw and stats data collection. Disable tracing and UMA.
CastLoggingConfig GetDefaultCastSenderLoggingConfig();
CastLoggingConfig GetDefaultCastReceiverLoggingConfig();

enum CastLoggingEvent {
  // Generic events.
  kUnknown,
  kRttMs,
  kPacketLoss,
  kJitterMs,
  kVideoAckReceived,
  kRembBitrate,
  kAudioAckSent,
  kVideoAckSent,
  // Audio sender.
  kAudioFrameReceived,
  kAudioFrameCaptured,
  kAudioFrameEncoded,
  // Audio receiver.
  kAudioPlayoutDelay,
  kAudioFrameDecoded,
  // Video sender.
  kVideoFrameCaptured,
  kVideoFrameReceived,
  kVideoFrameSentToEncoder,
  kVideoFrameEncoded,
  // Video receiver.
  kVideoFrameDecoded,
  kVideoRenderDelay,
  // Send-side packet events.
  kPacketSentToPacer,
  kPacketSentToNetwork,
  kPacketRetransmitted,
  // Receive-side packet events.
  kAudioPacketReceived,
  kVideoPacketReceived,
  kDuplicatePacketReceived,

  kNumOfLoggingEvents,
};

std::string CastLoggingToString(CastLoggingEvent event);

struct FrameEvent {
  FrameEvent();
  ~FrameEvent();

  uint32 frame_id;
  // Size is set only for kAudioFrameEncoded and kVideoFrameEncoded.
  size_t size;
  std::vector<base::TimeTicks> timestamp;
  std::vector<CastLoggingEvent> type;
  // Delay is only set for kAudioPlayoutDelay and kVideoRenderDelay.
  base::TimeDelta delay_delta;  // Render/playout delay.
};

// Internal map sorted by packet id.
struct BasePacketInfo {
  BasePacketInfo();
  ~BasePacketInfo();

  size_t size;
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

// Generic statistics given the raw data. More specific data (e.g. frame rate
// and bit rate) can be computed given the basic metrics.
// Some of the metrics will only be set when applicable, e.g. delay and size.
struct FrameLogStats {
  FrameLogStats();
  ~FrameLogStats();
  base::TimeTicks first_event_time;
  base::TimeTicks last_event_time;
  int event_counter;
  size_t sum_size;
  base::TimeDelta min_delay;
  base::TimeDelta max_delay;
  base::TimeDelta sum_delay;
};

struct PacketLogStats {
  PacketLogStats();
  ~PacketLogStats();
  base::TimeTicks first_event_time;
  base::TimeTicks last_event_time;
  int event_counter;
  size_t sum_size;
};

struct GenericLogStats {
  GenericLogStats();
  ~GenericLogStats();
  base::TimeTicks first_event_time;
  base::TimeTicks last_event_time;
  int event_counter;
  int sum;
  uint64 sum_squared;
  int min;
  int max;
};

struct ReceiverRtcpEvent {
  ReceiverRtcpEvent();
  ~ReceiverRtcpEvent();

  CastLoggingEvent type;
  base::TimeTicks timestamp;
  base::TimeDelta delay_delta;  // Render/playout delay.
  uint16 packet_id;
};

// Store all log types in a map based on the event.
typedef std::map<uint32, FrameEvent> FrameRawMap;
typedef std::map<uint32, PacketEvent> PacketRawMap;
typedef std::map<CastLoggingEvent, GenericEvent> GenericRawMap;

typedef std::multimap<uint32, ReceiverRtcpEvent> AudioRtcpRawMap;
typedef std::multimap<uint32, ReceiverRtcpEvent> VideoRtcpRawMap;

typedef std::map<CastLoggingEvent, FrameLogStats> FrameStatsMap;
typedef std::map<CastLoggingEvent, PacketLogStats> PacketStatsMap;
typedef std::map<CastLoggingEvent, GenericLogStats> GenericStatsMap;

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_LOGGING_LOGGING_DEFINES_H_
