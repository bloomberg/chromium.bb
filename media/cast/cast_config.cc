// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/cast_config.h"

namespace media {
namespace cast {

VideoSenderConfig::VideoSenderConfig()
    : rtcp_interval(kDefaultRtcpIntervalMs),
      rtcp_mode(kRtcpReducedSize),
      rtp_history_ms(kDefaultRtpHistoryMs),
      rtp_max_delay_ms(kDefaultRtpMaxDelayMs),
      congestion_control_back_off(kDefaultCongestionControlBackOff),
      max_qp(kDefaultMaxQp),
      min_qp(kDefaultMinQp),
      max_frame_rate(kDefaultMaxFrameRate),
      max_number_of_video_buffers_used(kDefaultNumberOfVideoBuffers) {}

AudioSenderConfig::AudioSenderConfig()
    : rtcp_interval(kDefaultRtcpIntervalMs),
      rtcp_mode(kRtcpReducedSize),
      rtp_history_ms(kDefaultRtpHistoryMs),
      rtp_max_delay_ms(kDefaultRtpMaxDelayMs) {}

AudioReceiverConfig::AudioReceiverConfig()
    : feedback_ssrc(0),
      incoming_ssrc(0),
      rtcp_interval(kDefaultRtcpIntervalMs),
      rtcp_mode(kRtcpReducedSize),
      rtp_max_delay_ms(kDefaultRtpMaxDelayMs),
      rtp_payload_type(0),
      use_external_decoder(false),
      frequency(0),
      channels(0) {}

VideoReceiverConfig::VideoReceiverConfig()
    : feedback_ssrc(0),
      incoming_ssrc(0),
      rtcp_interval(kDefaultRtcpIntervalMs),
      rtcp_mode(kRtcpReducedSize),
      rtp_max_delay_ms(kDefaultRtpMaxDelayMs),
      rtp_payload_type(0),
      use_external_decoder(false),
      max_frame_rate(kDefaultMaxFrameRate),
      decoder_faster_than_max_frame_rate(true) {}

EncodedVideoFrame::EncodedVideoFrame() {}
EncodedVideoFrame::~EncodedVideoFrame() {}

EncodedAudioFrame::EncodedAudioFrame() {}
EncodedAudioFrame::~EncodedAudioFrame() {}

PcmAudioFrame::PcmAudioFrame() {}
PcmAudioFrame::~PcmAudioFrame() {}

// static
void PacketReceiver::DeletePacket(const uint8* packet) {
  delete [] packet;
}

}  // namespace cast
}  // namespace media
