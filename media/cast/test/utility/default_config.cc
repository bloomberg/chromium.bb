// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/utility/default_config.h"

#include "base/bind.h"
#include "media/cast/net/cast_transport_config.h"

namespace {

void CreateVideoEncodeAccelerator(
    const media::cast::ReceiveVideoEncodeAcceleratorCallback& callback) {
  // Do nothing.
}

void CreateVideoEncodeMemory(
    size_t size,
    const media::cast::ReceiveVideoEncodeMemoryCallback& callback) {
  // Do nothing.
}

}  // namespace

namespace media {
namespace cast {

FrameReceiverConfig GetDefaultAudioReceiverConfig() {
  FrameReceiverConfig config;
  config.receiver_ssrc = 2;
  config.sender_ssrc = 1;
  config.rtp_max_delay_ms = kDefaultRtpMaxDelayMs;
  config.rtp_payload_type = 127;
  config.rtp_timebase = 48000;
  config.channels = 2;
  config.target_frame_rate = 100;  // 10ms of signal per frame
  config.codec = media::cast::CODEC_AUDIO_OPUS;
  return config;
}

FrameReceiverConfig GetDefaultVideoReceiverConfig() {
  FrameReceiverConfig config;
  config.receiver_ssrc = 12;
  config.sender_ssrc = 11;
  config.rtp_max_delay_ms = kDefaultRtpMaxDelayMs;
  config.rtp_payload_type = 96;
  config.rtp_timebase = kVideoFrequency;
  config.channels = 1;
  config.target_frame_rate = kDefaultMaxFrameRate;
  config.codec = media::cast::CODEC_VIDEO_VP8;
  return config;
}

AudioSenderConfig GetDefaultAudioSenderConfig() {
  FrameReceiverConfig recv_config = GetDefaultAudioReceiverConfig();
  AudioSenderConfig config;
  config.ssrc = recv_config.sender_ssrc;
  config.receiver_ssrc = recv_config.receiver_ssrc;
  config.rtp_payload_type = recv_config.rtp_payload_type;
  config.use_external_encoder = false;
  config.frequency = recv_config.rtp_timebase;
  config.channels = recv_config.channels;
  config.bitrate = kDefaultAudioEncoderBitrate;
  config.codec = recv_config.codec;
  config.max_playout_delay =
      base::TimeDelta::FromMilliseconds(kDefaultRtpMaxDelayMs);
  return config;
}

VideoSenderConfig GetDefaultVideoSenderConfig() {
  FrameReceiverConfig recv_config = GetDefaultVideoReceiverConfig();
  VideoSenderConfig config;
  config.ssrc = recv_config.sender_ssrc;
  config.receiver_ssrc = recv_config.receiver_ssrc;
  config.rtp_payload_type = recv_config.rtp_payload_type;
  config.use_external_encoder = false;
  config.max_bitrate = 4000000;
  config.min_bitrate = 2000000;
  config.start_bitrate = 4000000;
  config.max_frame_rate = recv_config.target_frame_rate;
  config.max_number_of_video_buffers_used = 1;
  config.codec = recv_config.codec;
  config.number_of_encode_threads = 2;
  config.max_playout_delay =
      base::TimeDelta::FromMilliseconds(kDefaultRtpMaxDelayMs);
  return config;
}

CreateVideoEncodeAcceleratorCallback
CreateDefaultVideoEncodeAcceleratorCallback() {
  return base::Bind(&CreateVideoEncodeAccelerator);
}

CreateVideoEncodeMemoryCallback CreateDefaultVideoEncodeMemoryCallback() {
  return base::Bind(&CreateVideoEncodeMemory);
}

}  // namespace cast
}  // namespace media
