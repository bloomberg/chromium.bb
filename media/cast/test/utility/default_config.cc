// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/utility/default_config.h"

#include "base/bind.h"
#include "media/cast/transport/cast_transport_config.h"

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
  config.feedback_ssrc = 2;
  config.incoming_ssrc = 1;
  config.rtcp_c_name = "audio_receiver@a.b.c.d";
  config.rtp_max_delay_ms = kDefaultRtpMaxDelayMs;
  config.rtp_payload_type = 127;
  config.frequency = 48000;
  config.channels = 2;
  config.max_frame_rate = 100;  // 10ms of signal per frame
  config.codec.audio = media::cast::transport::kOpus;
  return config;
}

FrameReceiverConfig GetDefaultVideoReceiverConfig() {
  FrameReceiverConfig config;
  config.feedback_ssrc = 12;
  config.incoming_ssrc = 11;
  config.rtcp_c_name = "video_receiver@a.b.c.d";
  config.rtp_max_delay_ms = kDefaultRtpMaxDelayMs;
  config.rtp_payload_type = 96;
  config.frequency = kVideoFrequency;
  config.channels = 1;
  config.max_frame_rate = kDefaultMaxFrameRate;
  config.codec.video = media::cast::transport::kVp8;
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
