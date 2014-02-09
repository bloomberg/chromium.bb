// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/logging_defines.h"

#include "base/logging.h"

#define ENUM_TO_STRING(enum) \
  case k##enum:              \
    return #enum

namespace media {
namespace cast {

CastLoggingConfig::CastLoggingConfig(bool sender)
    : is_sender(sender),
      enable_raw_data_collection(false),
      enable_stats_data_collection(false),
      enable_uma_stats(false),
      enable_tracing(false) {}

CastLoggingConfig::~CastLoggingConfig() {}

CastLoggingConfig GetDefaultCastSenderLoggingConfig() {
  CastLoggingConfig config(true);
  return config;
}

CastLoggingConfig GetDefaultCastReceiverLoggingConfig() {
  CastLoggingConfig config(false);
  return config;
}

CastLoggingConfig GetLoggingConfigWithRawEventsAndStatsEnabled() {
  CastLoggingConfig config(true);
  config.enable_raw_data_collection = true;
  config.enable_stats_data_collection = true;
  return config;
}

std::string CastLoggingToString(CastLoggingEvent event) {
  switch (event) {
    // Can happen if the sender and receiver of RTCP log messages are not
    // aligned.
    ENUM_TO_STRING(Unknown);
    ENUM_TO_STRING(RttMs);
    ENUM_TO_STRING(PacketLoss);
    ENUM_TO_STRING(JitterMs);
    ENUM_TO_STRING(VideoAckReceived);
    ENUM_TO_STRING(RembBitrate);
    ENUM_TO_STRING(AudioAckSent);
    ENUM_TO_STRING(VideoAckSent);
    ENUM_TO_STRING(AudioFrameReceived);
    ENUM_TO_STRING(AudioFrameCaptured);
    ENUM_TO_STRING(AudioFrameEncoded);
    ENUM_TO_STRING(AudioPlayoutDelay);
    ENUM_TO_STRING(AudioFrameDecoded);
    ENUM_TO_STRING(VideoFrameCaptured);
    ENUM_TO_STRING(VideoFrameReceived);
    ENUM_TO_STRING(VideoFrameSentToEncoder);
    ENUM_TO_STRING(VideoFrameEncoded);
    ENUM_TO_STRING(VideoFrameDecoded);
    ENUM_TO_STRING(VideoRenderDelay);
    ENUM_TO_STRING(PacketSentToPacer);
    ENUM_TO_STRING(PacketSentToNetwork);
    ENUM_TO_STRING(PacketRetransmitted);
    ENUM_TO_STRING(AudioPacketReceived);
    ENUM_TO_STRING(VideoPacketReceived);
    ENUM_TO_STRING(DuplicatePacketReceived);
    case kNumOfLoggingEvents:
      NOTREACHED();
      return "";
  }
  NOTREACHED();
  return "";
}

FrameEvent::FrameEvent()
    : rtp_timestamp(0u), frame_id(kFrameIdUnknown), size(0u), type(kUnknown) {}
FrameEvent::~FrameEvent() {}

PacketEvent::PacketEvent()
    : rtp_timestamp(0),
      frame_id(kFrameIdUnknown),
      max_packet_id(0),
      packet_id(0),
      size(0),
      type(kUnknown) {}
PacketEvent::~PacketEvent() {}

GenericEvent::GenericEvent() : type(kUnknown), value(0) {}
GenericEvent::~GenericEvent() {}

FrameLogStats::FrameLogStats()
    : event_counter(0),
      sum_size(0) {}
FrameLogStats::~FrameLogStats() {}

PacketLogStats::PacketLogStats()
    : event_counter(0),
      sum_size(0) {}
PacketLogStats::~PacketLogStats() {}

GenericLogStats::GenericLogStats()
    : event_counter(0),
      sum(0),
      sum_squared(0),
      min(0),
      max(0) {}
GenericLogStats::~GenericLogStats() {}
}  // namespace cast
}  // namespace media

