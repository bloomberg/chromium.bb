// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_LOGGING_LOGGING_DEFINES_H_
#define MEDIA_CAST_LOGGING_LOGGING_DEFINES_H_

#include "base/logging.h"

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

std::string CastEnumToString(CastLoggingEvent event) {
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

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_LOGGING_LOGGING_DEFINES_H_
