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

typedef uint32 RtpTimestamp;

enum CastLoggingEvent {
  kUnknown,
  // Generic events. These are no longer used.
  kRttMs,
  kPacketLoss,
  kJitterMs,
  kVideoAckReceived,  // Sender side frame event.
  kRembBitrate,  // Generic event. No longer used.
  // Receiver side frame events.
  kAudioAckSent,
  kVideoAckSent,
  // Audio sender.
  kAudioFrameReceived,
  kAudioFrameCaptured,
  kAudioFrameEncoded,
  // Audio receiver.
  kAudioFrameDecoded,
  kAudioPlayoutDelay,
  // Video sender.
  kVideoFrameCaptured,
  kVideoFrameReceived,
  kVideoFrameSentToEncoder,
  kVideoFrameEncoded,
  // Video receiver.
  kVideoFrameDecoded,
  kVideoRenderDelay,
  // Send-side packet events.
  kAudioPacketSentToPacer,
  kVideoPacketSentToPacer,
  kAudioPacketSentToNetwork,
  kVideoPacketSentToNetwork,
  kAudioPacketRetransmitted,
  kVideoPacketRetransmitted,
  // Receive-side packet events.
  kAudioPacketReceived,
  kVideoPacketReceived,
  kDuplicateAudioPacketReceived,
  kDuplicateVideoPacketReceived,
  kNumOfLoggingEvents = kDuplicateVideoPacketReceived
};

const char* CastLoggingToString(CastLoggingEvent event);

// CastLoggingEvent are classified into one of three following types.
enum EventMediaType { AUDIO_EVENT, VIDEO_EVENT, OTHER_EVENT };

EventMediaType GetEventMediaType(CastLoggingEvent event);

struct FrameEvent {
  FrameEvent();
  ~FrameEvent();

  RtpTimestamp rtp_timestamp;
  uint32 frame_id;
  // Size of encoded frame. Only set for kVideoFrameEncoded event.
  size_t size;

  // Time of event logged.
  base::TimeTicks timestamp;

  CastLoggingEvent type;

  // Render / playout delay. Only set for kAudioPlayoutDelay and
  // kVideoRenderDelay events.
  base::TimeDelta delay_delta;

  // Whether the frame is a key frame. Only set for kVideoFrameEncoded event.
  bool key_frame;

  // The requested target bitrate of the encoder at the time the frame is
  // encoded. Only set for kVideoFrameEncoded event.
  int target_bitrate;
};

struct PacketEvent {
  PacketEvent();
  ~PacketEvent();

  RtpTimestamp rtp_timestamp;
  uint32 frame_id;
  uint16 max_packet_id;
  uint16 packet_id;
  size_t size;

  // Time of event logged.
  base::TimeTicks timestamp;
  CastLoggingEvent type;
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

typedef std::map<CastLoggingEvent, FrameLogStats> FrameStatsMap;
typedef std::map<CastLoggingEvent, PacketLogStats> PacketStatsMap;

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_LOGGING_LOGGING_DEFINES_H_
