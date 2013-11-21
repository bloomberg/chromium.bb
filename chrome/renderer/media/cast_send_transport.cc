// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/cast_send_transport.h"

#include "base/logging.h"
#include "chrome/renderer/media/cast_session.h"
#include "chrome/renderer/media/cast_udp_transport.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_defines.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"

using media::cast::AudioSenderConfig;
using media::cast::VideoSenderConfig;

namespace {
const char kCodecNameOpus[] = "OPUS";
const char kCodecNameVp8[] = "VP8";

CastRtpPayloadParams DefaultOpusPayload() {
  CastRtpPayloadParams payload;
  payload.payload_type = 111;
  payload.codec_name = kCodecNameOpus;
  payload.clock_rate = 48000;
  payload.channels = 2;
  payload.min_bitrate = payload.max_bitrate =
      media::cast::kDefaultAudioEncoderBitrate;
  return payload;
}

CastRtpPayloadParams DefaultVp8Payload() {
  CastRtpPayloadParams payload;
  payload.payload_type = 100;
  payload.codec_name = kCodecNameVp8;
  payload.clock_rate = 90000;
  payload.width = 1280;
  payload.height = 720;
  payload.min_bitrate = 50 * 1000;
  payload.max_bitrate = 2000 * 1000;
  return payload;
}

CastRtpCaps DefaultAudioCaps() {
  CastRtpCaps caps;
  caps.payloads.push_back(DefaultOpusPayload());
  // TODO(hclam): Fill in |rtcp_features| and |fec_mechanisms|.
  return caps;
}

CastRtpCaps DefaultVideoCaps() {
  CastRtpCaps caps;
  caps.payloads.push_back(DefaultVp8Payload());
  // TODO(hclam): Fill in |rtcp_features| and |fec_mechanisms|.
  return caps;
}

bool ToAudioSenderConfig(const CastRtpParams& params,
                         AudioSenderConfig* config) {
  if (params.payloads.empty())
    return false;
  const CastRtpPayloadParams& payload_params = params.payloads[0];
  config->sender_ssrc = payload_params.ssrc;
  config->use_external_encoder = false;
  config->frequency = payload_params.clock_rate;
  config->channels = payload_params.channels;
  config->bitrate = payload_params.max_bitrate;
  config->codec = media::cast::kPcm16;
  if (payload_params.codec_name == kCodecNameOpus)
    config->codec = media::cast::kOpus;
  else
    return false;
  return true;
}

bool ToVideoSenderConfig(const CastRtpParams& params,
                         VideoSenderConfig* config) {
  if (params.payloads.empty())
    return false;
  const CastRtpPayloadParams& payload_params = params.payloads[0];
  config->sender_ssrc = payload_params.ssrc;
  config->use_external_encoder = false;
  config->width = payload_params.width;
  config->height = payload_params.height;
  config->min_bitrate = config->start_bitrate = payload_params.min_bitrate;
  config->max_bitrate = payload_params.max_bitrate;
  if (payload_params.codec_name == kCodecNameVp8)
    config->codec = media::cast::kVp8;
  else
    return false;
  return true;
}
}  // namespace

CastCodecSpecificParams::CastCodecSpecificParams() {
}

CastCodecSpecificParams::~CastCodecSpecificParams() {
}

CastRtpPayloadParams::CastRtpPayloadParams()
    : payload_type(0),
      ssrc(0),
      clock_rate(0),
      max_bitrate(0),
      min_bitrate(0),
      channels(0),
      width(0),
      height(0) {
}

CastRtpPayloadParams::~CastRtpPayloadParams() {
}

CastRtpCaps::CastRtpCaps() {
}

CastRtpCaps::~CastRtpCaps() {
}

CastSendTransport::CastSendTransport(
    CastUdpTransport* udp_transport,
    const blink::WebMediaStreamTrack& track)
    : cast_session_(udp_transport->cast_session()), track_(track) {
}

CastSendTransport::~CastSendTransport() {
}

CastRtpCaps CastSendTransport::GetCaps() {
  if (IsAudio())
    return DefaultAudioCaps();
  else
    return DefaultVideoCaps();
}

CastRtpParams CastSendTransport::GetParams() {
  return params_;
}

CastRtpParams CastSendTransport::CreateParams(
    const CastRtpCaps& remote_caps) {
  // TODO(hclam): Filter out the parameters based on remote capabilities.
  // For now always return the default caps as parameters.
  return GetCaps();
}

void CastSendTransport::Start(const CastRtpParams& params) {
  if (IsAudio()) {
    AudioSenderConfig config;
    if (!ToAudioSenderConfig(params, &config)) {
      DVLOG(1) << "Invalid parameters for audio.";
    }
    cast_session_->StartAudio(config);
  } else {
    VideoSenderConfig config;
    if (!ToVideoSenderConfig(params, &config)) {
      DVLOG(1) << "Invalid parameters for video.";
    }
    cast_session_->StartVideo(config);
  }
}

void CastSendTransport::Stop() {
  NOTIMPLEMENTED();
}

bool CastSendTransport::IsAudio() const {
  return track_.source().type() == blink::WebMediaStreamSource::TypeAudio;
}
