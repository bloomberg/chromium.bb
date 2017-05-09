// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/gpu/rtc_video_encoder_factory.h"

#include "base/command_line.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/feature_h264_with_openh264_ffmpeg.h"
#include "content/renderer/media/gpu/rtc_video_encoder.h"
#include "media/gpu/ipc/client/gpu_video_encode_accelerator_host.h"
#include "media/renderers/gpu_video_accelerator_factories.h"
#include "media/video/video_encode_accelerator.h"

namespace content {

namespace {

// Translate from media::VideoEncodeAccelerator::SupportedProfile to
// one or more instances of cricket::WebRtcVideoEncoderFactory::VideoCodec
void VEAToWebRTCCodecs(
    std::vector<cricket::VideoCodec>* codecs,
    const media::VideoEncodeAccelerator::SupportedProfile& profile) {
  DCHECK_EQ(profile.max_framerate_denominator, 1U);

  if (profile.profile >= media::VP8PROFILE_MIN &&
      profile.profile <= media::VP8PROFILE_MAX) {
    if (base::FeatureList::IsEnabled(features::kWebRtcHWVP8Encoding)) {
      codecs->push_back(cricket::VideoCodec("VP8"));
    }
  } else if (profile.profile >= media::H264PROFILE_MIN &&
             profile.profile <= media::H264PROFILE_MAX) {
    // Enable H264 HW encode for WebRTC when SW fallback is available, which is
    // checked by kWebRtcH264WithOpenH264FFmpeg flag. This check should be
    // removed when SW implementation is fully enabled.
    bool webrtc_h264_sw_enabled = false;
#if BUILDFLAG(RTC_USE_H264) && !defined(MEDIA_DISABLE_FFMPEG)
    webrtc_h264_sw_enabled =
        base::FeatureList::IsEnabled(kWebRtcH264WithOpenH264FFmpeg);
#endif  // BUILDFLAG(RTC_USE_H264) && !defined(MEDIA_DISABLE_FFMPEG)
    if (webrtc_h264_sw_enabled ||
        base::FeatureList::IsEnabled(features::kWebRtcHWH264Encoding)) {
      // TODO(magjed): Propagate H264 profile information.
      codecs->push_back(cricket::VideoCodec("H264"));
    }
  }
}

}  // anonymous namespace

RTCVideoEncoderFactory::RTCVideoEncoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories)
    : gpu_factories_(gpu_factories) {
  const media::VideoEncodeAccelerator::SupportedProfiles& profiles =
      gpu_factories_->GetVideoEncodeAcceleratorSupportedProfiles();
  for (const auto& profile : profiles)
    VEAToWebRTCCodecs(&supported_codecs_, profile);
}

RTCVideoEncoderFactory::~RTCVideoEncoderFactory() {}

webrtc::VideoEncoder* RTCVideoEncoderFactory::CreateVideoEncoder(
    const cricket::VideoCodec& codec) {
  for (const cricket::VideoCodec& supported_codec : supported_codecs_) {
    if (cricket::CodecNamesEq(codec.name, supported_codec.name)) {
      webrtc::VideoCodecType type = webrtc::PayloadNameToCodecType(codec.name)
                                        .value_or(webrtc::kVideoCodecUnknown);
      return new RTCVideoEncoder(type, gpu_factories_);
    }
  }
  return nullptr;
}

const std::vector<cricket::VideoCodec>&
RTCVideoEncoderFactory::supported_codecs() const {
  return supported_codecs_;
}

void RTCVideoEncoderFactory::DestroyVideoEncoder(
    webrtc::VideoEncoder* encoder) {
  delete encoder;
}

}  // namespace content
