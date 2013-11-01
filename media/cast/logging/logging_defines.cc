// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/logging_defines.h"

#include "base/logging.h"

namespace media {
namespace cast {

std::string CastLoggingToString(CastLoggingEvent event) {
  switch (event) {
    case(kRtt):
      return "Rtt";
    case(kPacketLoss):
      return "PacketLoss";
    case(kJitter):
      return "Jitter";
    case(kAckReceived):
      return "AckReceived";
    case(kAckSent):
      return "AckSent";
    case(kLastEvent):
      return "LastEvent";
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
    case(kPacketRetransmited):
      return "PacketRetransmited";
    case(kPacketReceived):
      return "PacketReceived";
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

FrameLogStats::FrameLogStats()
    : framerate_fps(0),
      bitrate_kbps(0),
      max_delay_ms(0),
      min_delay_ms(0),
      avg_delay_ms(0) {}
FrameLogStats::~FrameLogStats() {}

}  // namespace cast
}  // namespace media
