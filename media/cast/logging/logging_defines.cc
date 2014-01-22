// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/logging_defines.h"

#include "base/logging.h"

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

std::string CastLoggingToString(CastLoggingEvent event) {
  switch (event) {
    case(kUnknown):
      // Can happen if the sender and receiver of RTCP log messages are not
      // aligned.
      return "Unknown";
    case(kRttMs):
      return "RttMs";
    case(kPacketLoss):
      return "PacketLoss";
    case(kJitterMs):
      return "JitterMs";
    case(kVideoAckReceived):
      return "VideoAckReceived";
    case(kRembBitrate):
      return "RembBitrate";
    case(kAudioAckSent):
      return "AudioAckSent";
    case(kVideoAckSent):
      return "VideoAckSent";
    case(kAudioFrameReceived):
      return "AudioFrameReceived";
    case(kAudioFrameCaptured):
      return "AudioFrameCaptured";
    case(kAudioFrameEncoded):
      return "AudioFrameEncoded";
    case(kAudioPlayoutDelay):
      return "AudioPlayoutDelay";
    case(kAudioFrameDecoded):
      return "AudioFrameDecoded";
    case(kVideoFrameCaptured):
      return "VideoFrameCaptured";
    case(kVideoFrameReceived):
      return "VideoFrameReceived";
    case(kVideoFrameSentToEncoder):
      return "VideoFrameSentToEncoder";
    case(kVideoFrameEncoded):
      return "VideoFrameEncoded";
    case(kVideoFrameDecoded):
      return "VideoFrameDecoded";
    case(kVideoRenderDelay):
      return "VideoRenderDelay";
    case(kPacketSentToPacer):
      return "PacketSentToPacer";
    case(kPacketSentToNetwork):
      return "PacketSentToNetwork";
    case(kPacketRetransmitted):
      return "PacketRetransmited";
    case(kAudioPacketReceived):
      return "AudioPacketReceived";
    case(kVideoPacketReceived):
      return "VideoPacketReceived";
    case(kDuplicatePacketReceived):
      return "DuplicatePacketReceived";
    default:
      NOTREACHED();
      return "";
  }
}

FrameEvent::FrameEvent() {}
FrameEvent::~FrameEvent() {}

BasePacketInfo::BasePacketInfo() {}
BasePacketInfo::~BasePacketInfo() {}

PacketEvent::PacketEvent() {}
PacketEvent::~PacketEvent() {}

GenericEvent::GenericEvent() {}
GenericEvent::~GenericEvent() {}

ReceiverRtcpEvent::ReceiverRtcpEvent() {}
ReceiverRtcpEvent::~ReceiverRtcpEvent() {}

FrameLogStats::FrameLogStats()
    : framerate_fps(0),
      bitrate_kbps(0),
      max_delay_ms(0),
      min_delay_ms(0),
      avg_delay_ms(0) {}
FrameLogStats::~FrameLogStats() {}

}  // namespace cast
}  // namespace media
