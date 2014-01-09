// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/transport/cast_transport_config.h"

namespace media {
namespace cast {
namespace transport {

namespace {
  const int kDefaultRtpHistoryMs = 1000;
  const int kDefaultRtpMaxDelayMs = 100;
}  // namespace

CastTransportConfig::CastTransportConfig()
    : sender_ssrc(0),
      rtp_history_ms(kDefaultRtpHistoryMs),
      rtp_max_delay_ms(kDefaultRtpMaxDelayMs),
      rtp_payload_type(0),
      frequency(0),
      channels(0),
      aes_key (""),
      aes_iv_mask(0) {}

CastTransportConfig::~CastTransportConfig() {}

EncodedVideoFrame::EncodedVideoFrame() {}
EncodedVideoFrame::~EncodedVideoFrame() {}

EncodedAudioFrame::EncodedAudioFrame() {}
EncodedAudioFrame::~EncodedAudioFrame() {}

// static
void PacketReceiver::DeletePacket(const uint8* packet) {
  delete [] packet;
}

}  // namespace transport
}  // namespace cast
}  // namespace media
