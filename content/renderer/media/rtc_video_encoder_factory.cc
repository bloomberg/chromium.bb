// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_video_encoder_factory.h"

#include "base/command_line.h"
#include "content/common/gpu/client/gpu_video_encode_accelerator_host.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/media/rtc_video_encoder.h"
#include "media/filters/gpu_video_accelerator_factories.h"
#include "media/video/video_encode_accelerator.h"

namespace content {

namespace {

// Translate from media::VideoEncodeAccelerator::SupportedProfile to
// one or more instances of cricket::WebRtcVideoEncoderFactory::VideoCodec
void VEAToWebRTCCodecs(
    std::vector<cricket::WebRtcVideoEncoderFactory::VideoCodec>* codecs,
    const media::VideoEncodeAccelerator::SupportedProfile& profile) {
  int width = profile.max_resolution.width();
  int height = profile.max_resolution.height();
  int fps = profile.max_framerate_numerator;
  DCHECK_EQ(profile.max_framerate_denominator, 1U);

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (profile.profile >= media::VP8PROFILE_MIN &&
      profile.profile <= media::VP8PROFILE_MAX) {
    if (cmd_line->HasSwitch(switches::kEnableWebRtcHWVp8Encoding)) {
      codecs->push_back(cricket::WebRtcVideoEncoderFactory::VideoCodec(
          webrtc::kVideoCodecVP8, "VP8", width, height, fps));
    }
  } else if (profile.profile >= media::H264PROFILE_MIN &&
             profile.profile <= media::H264PROFILE_MAX) {
    if (cmd_line->HasSwitch(switches::kEnableWebRtcHWH264Encoding)) {
      codecs->push_back(cricket::WebRtcVideoEncoderFactory::VideoCodec(
          webrtc::kVideoCodecH264, "H264", width, height, fps));
    }
    // TODO(hshi): remove the generic codec type after CASTv1 deprecation.
    codecs->push_back(cricket::WebRtcVideoEncoderFactory::VideoCodec(
        webrtc::kVideoCodecGeneric, "CAST1", width, height, fps));
  }
}

// Translate from cricket::WebRtcVideoEncoderFactory::VideoCodec to
// media::VideoCodecProfile.  Pick a default profile for each codec type.
media::VideoCodecProfile WebRTCCodecToVideoCodecProfile(
    webrtc::VideoCodecType type) {
  switch (type) {
    case webrtc::kVideoCodecVP8:
      return media::VP8PROFILE_ANY;
    case webrtc::kVideoCodecH264:
    case webrtc::kVideoCodecGeneric:
      return media::H264PROFILE_MAIN;
    default:
      return media::VIDEO_CODEC_PROFILE_UNKNOWN;
  }
}

}  // anonymous namespace

RTCVideoEncoderFactory::RTCVideoEncoderFactory(
    const scoped_refptr<media::GpuVideoAcceleratorFactories>& gpu_factories)
    : gpu_factories_(gpu_factories) {
  std::vector<media::VideoEncodeAccelerator::SupportedProfile> profiles =
      gpu_factories_->GetVideoEncodeAcceleratorSupportedProfiles();
  for (size_t i = 0; i < profiles.size(); ++i)
    VEAToWebRTCCodecs(&codecs_, profiles[i]);
}

RTCVideoEncoderFactory::~RTCVideoEncoderFactory() {}

webrtc::VideoEncoder* RTCVideoEncoderFactory::CreateVideoEncoder(
    webrtc::VideoCodecType type) {
  bool found = false;
  for (size_t i = 0; i < codecs_.size(); ++i) {
    if (codecs_[i].type == type) {
      found = true;
      break;
    }
  }
  if (!found)
    return NULL;
  return new RTCVideoEncoder(
      type, WebRTCCodecToVideoCodecProfile(type), gpu_factories_);
}

void RTCVideoEncoderFactory::AddObserver(Observer* observer) {
  // No-op: our codec list is populated on installation.
}

void RTCVideoEncoderFactory::RemoveObserver(Observer* observer) {}

const std::vector<cricket::WebRtcVideoEncoderFactory::VideoCodec>&
RTCVideoEncoderFactory::codecs() const {
  return codecs_;
}

void RTCVideoEncoderFactory::DestroyVideoEncoder(
    webrtc::VideoEncoder* encoder) {
  delete encoder;
}

}  // namespace content
