// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/proto/proto_utils.h"

#include "base/logging.h"

#define TO_PROTO_ENUM(from_enum, to_enum)  \
  case from_enum:                          \
    return media::cast::proto::to_enum

namespace media {
namespace cast {

media::cast::proto::EventType ToProtoEventType(CastLoggingEvent event) {
  switch (event) {
    TO_PROTO_ENUM(kUnknown, UNKNOWN);
    TO_PROTO_ENUM(kRttMs, RTT_MS);
    TO_PROTO_ENUM(kPacketLoss, PACKET_LOSS);
    TO_PROTO_ENUM(kJitterMs, JITTER_MS);
    TO_PROTO_ENUM(kVideoAckReceived, VIDEO_ACK_RECEIVED);
    TO_PROTO_ENUM(kRembBitrate, REMB_BITRATE);
    TO_PROTO_ENUM(kAudioAckSent, AUDIO_ACK_SENT);
    TO_PROTO_ENUM(kVideoAckSent, VIDEO_ACK_SENT);
    TO_PROTO_ENUM(kAudioFrameReceived, AUDIO_FRAME_RECEIVED);
    TO_PROTO_ENUM(kAudioFrameCaptured, AUDIO_FRAME_CAPTURED);
    TO_PROTO_ENUM(kAudioFrameEncoded, AUDIO_FRAME_ENCODED);
    TO_PROTO_ENUM(kAudioPlayoutDelay, AUDIO_PLAYOUT_DELAY);
    TO_PROTO_ENUM(kAudioFrameDecoded, AUDIO_FRAME_DECODED);
    TO_PROTO_ENUM(kVideoFrameCaptured, VIDEO_FRAME_CAPTURED);
    TO_PROTO_ENUM(kVideoFrameReceived, VIEDO_FRAME_RECEIVED);
    TO_PROTO_ENUM(kVideoFrameSentToEncoder, VIDEO_FRAME_SENT_TO_ENCODER);
    TO_PROTO_ENUM(kVideoFrameEncoded, VIDEO_FRAME_ENCODED);
    TO_PROTO_ENUM(kVideoFrameDecoded, VIDEO_FRAME_DECODED);
    TO_PROTO_ENUM(kVideoRenderDelay, VIDEO_RENDER_DELAY);
    TO_PROTO_ENUM(kAudioPacketSentToPacer, AUDIO_PACKET_SENT_TO_PACER);
    TO_PROTO_ENUM(kVideoPacketSentToPacer, VIDEO_PACKET_SENT_TO_PACER);
    TO_PROTO_ENUM(kAudioPacketSentToNetwork, AUDIO_PACKET_SENT_TO_NETWORK);
    TO_PROTO_ENUM(kVideoPacketSentToNetwork, VIDEO_PACKET_SENT_TO_NETWORK);
    TO_PROTO_ENUM(kAudioPacketRetransmitted, AUDIO_PACKET_RETRANSMITTED);
    TO_PROTO_ENUM(kVideoPacketRetransmitted, VIDEO_PACKET_RETRANSMITTED);
    TO_PROTO_ENUM(kAudioPacketReceived, AUDIO_PACKET_RECEIVED);
    TO_PROTO_ENUM(kVideoPacketReceived, VIDEO_PACKET_RECEIVED);
    TO_PROTO_ENUM(kDuplicateAudioPacketReceived,
                  DUPLICATE_AUDIO_PACKET_RECEIVED);
    TO_PROTO_ENUM(kDuplicateVideoPacketReceived,
                  DUPLICATE_VIDEO_PACKET_RECEIVED);
  }
  NOTREACHED();
  return media::cast::proto::UNKNOWN;
}

}  // namespace cast
}  // namespace media
