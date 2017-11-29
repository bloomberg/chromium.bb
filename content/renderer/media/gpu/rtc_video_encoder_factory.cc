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
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/common_video/h264/profile_level_id.h"
#include "third_party/webrtc/media/base/codec.h"

namespace content {

namespace {

// Translate from media::VideoEncodeAccelerator::SupportedProfile to
// webrtc::SdpVideoFormat, or return nothing if the profile isn't supported.
base::Optional<webrtc::SdpVideoFormat> VEAToWebRTCFormat(
    const media::VideoEncodeAccelerator::SupportedProfile& profile) {
  DCHECK_EQ(profile.max_framerate_denominator, 1U);

  if (profile.profile >= media::VP8PROFILE_MIN &&
      profile.profile <= media::VP8PROFILE_MAX) {
    if (base::FeatureList::IsEnabled(features::kWebRtcHWVP8Encoding)) {
      return webrtc::SdpVideoFormat("VP8");
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
      webrtc::H264::Profile h264_profile;
      switch (profile.profile) {
        case media::H264PROFILE_BASELINE:
          h264_profile = webrtc::H264::kProfileBaseline;
          break;
        case media::H264PROFILE_MAIN:
          h264_profile = webrtc::H264::kProfileMain;
          break;
        case media::H264PROFILE_HIGH:
          h264_profile = webrtc::H264::kProfileHigh;
          break;
        default:
          // Unsupported H264 profile in WebRTC.
          return base::nullopt;
      }

      const int width = profile.max_resolution.width();
      const int height = profile.max_resolution.height();
      const int fps = profile.max_framerate_numerator;
      DCHECK_EQ(1u, profile.max_framerate_denominator);

      const rtc::Optional<webrtc::H264::Level> h264_level =
          webrtc::H264::SupportedLevel(width * height, fps);
      const webrtc::H264::ProfileLevelId profile_level_id(
          h264_profile, h264_level.value_or(webrtc::H264::kLevel1));

      webrtc::SdpVideoFormat format("H264");
      format.parameters = {
          {cricket::kH264FmtpProfileLevelId,
           *webrtc::H264::ProfileLevelIdToString(profile_level_id)},
          {cricket::kH264FmtpLevelAsymmetryAllowed, "1"},
          {cricket::kH264FmtpPacketizationMode, "1"}};
      return format;
    }
  }
  return base::nullopt;
}

bool IsSameFormat(const webrtc::SdpVideoFormat& format1,
                  const webrtc::SdpVideoFormat& format2) {
  return cricket::IsSameCodec(format1.name, format2.parameters, format2.name,
                              format2.parameters);
}

}  // anonymous namespace

RTCVideoEncoderFactory::RTCVideoEncoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories)
    : gpu_factories_(gpu_factories) {
  const media::VideoEncodeAccelerator::SupportedProfiles& profiles =
      gpu_factories_->GetVideoEncodeAcceleratorSupportedProfiles();
  for (const auto& profile : profiles) {
    base::Optional<webrtc::SdpVideoFormat> format = VEAToWebRTCFormat(profile);
    if (format) {
      supported_formats_.push_back(std::move(*format));
      profiles_.push_back(profile.profile);
    }
  }
  // There should be a 1:1 mapping between media::VideoCodecProfile and
  // webrtc::SdpVideoFormat.
  CHECK_EQ(profiles_.size(), supported_formats_.size());
}

RTCVideoEncoderFactory::~RTCVideoEncoderFactory() {}

std::unique_ptr<webrtc::VideoEncoder>
RTCVideoEncoderFactory::CreateVideoEncoder(
    const webrtc::SdpVideoFormat& format) {
  for (size_t i = 0; i < supported_formats_.size(); ++i) {
    if (IsSameFormat(format, supported_formats_[i])) {
      // There should be a 1:1 mapping between media::VideoCodecProfile and
      // webrtc::SdpVideoFormat.
      CHECK_EQ(profiles_.size(), supported_formats_.size());
      return base::MakeUnique<RTCVideoEncoder>(profiles_[i], gpu_factories_);
    }
  }
  return nullptr;
}

std::vector<webrtc::SdpVideoFormat>
RTCVideoEncoderFactory::GetSupportedFormats() const {
  return supported_formats_;
}

webrtc::VideoEncoderFactory::CodecInfo
RTCVideoEncoderFactory::QueryVideoEncoder(
    const webrtc::SdpVideoFormat& format) const {
  CodecInfo info;
  info.has_internal_source = false;
  info.is_hardware_accelerated = true;
  return info;
}

}  // namespace content
