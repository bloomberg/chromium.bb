// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_NET_RTP_SENDER_RTP_PACKETIZER_RTP_PACKETIZER_CONFIG_H_
#define CAST_NET_RTP_SENDER_RTP_PACKETIZER_RTP_PACKETIZER_CONFIG_H_

#include "media/cast/cast_config.h"
#include "media/cast/rtp_receiver/rtp_receiver_defines.h"

namespace media {
namespace cast {

struct RtpPacketizerConfig {
  RtpPacketizerConfig();

  // General.
  bool audio;
  int payload_type;
  uint16 max_payload_length;
  uint16 sequence_number;
  uint32 rtp_timestamp;
  int frequency;

  // SSRC.
  unsigned int ssrc;

  // Video.
  VideoCodec video_codec;

  // Audio.
  uint8 channels;
  AudioCodec audio_codec;
};

}  // namespace cast
}  // namespace media

#endif  // CAST_NET_RTP_SENDER_RTP_PACKETIZER_RTP_PACKETIZER_CONFIG_H_
