// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/utility/default_config.h"

#include "media/cast/transport/cast_transport_config.h"

namespace media {
namespace cast {

AudioReceiverConfig GetDefaultAudioReceiverConfig() {
  AudioReceiverConfig config;
  config.feedback_ssrc = 2;
  config.incoming_ssrc = 1;
  config.rtp_payload_type = 127;
  config.rtcp_c_name = "audio_receiver@a.b.c.d";
  config.use_external_decoder = false;
  config.frequency = 48000;
  config.channels = 2;
  config.codec = media::cast::transport::kOpus;
  return config;
}

VideoReceiverConfig GetDefaultVideoReceiverConfig() {
  VideoReceiverConfig config;
  config.feedback_ssrc = 12;
  config.incoming_ssrc = 11;
  config.rtp_payload_type = 96;
  config.rtcp_c_name = "video_receiver@a.b.c.d";
  config.use_external_decoder = false;
  config.codec = media::cast::transport::kVp8;
  return config;
}

}  // namespace cast
}  // namespace media
