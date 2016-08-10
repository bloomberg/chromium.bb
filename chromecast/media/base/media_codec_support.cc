// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/base/media_codec_support.h"

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "chromecast/media/base/media_caps.h"
#include "chromecast/public/media_codec_support_shlib.h"

namespace chromecast {
namespace media {
namespace {

bool IsCodecSupported(const std::string& codec) {
  // FLAC and Opus are supported via the default renderer if the CMA backend
  // does not have explicit support.
  if (codec == "opus" || codec == "flac")
    return true;

  MediaCodecSupportShlib::CodecSupport platform_support =
      MediaCodecSupportShlib::IsSupported(codec);
  if (platform_support == MediaCodecSupportShlib::kSupported)
    return true;
  else if (platform_support == MediaCodecSupportShlib::kNotSupported)
    return false;

  if (codec == "aac51") {
    return MediaCapabilities::HdmiSinkSupportsPcmSurroundSound();
  }
  if (codec == "ac-3" || codec == "mp4a.a5" || codec == "mp4a.A5") {
    return MediaCapabilities::HdmiSinkSupportsAC3();
  }
  if (codec == "ec-3" || codec == "mp4a.a6" || codec == "mp4a.A6") {
    return MediaCapabilities::HdmiSinkSupportsEAC3();
  }

  // This function is invoked from MimeUtil::AreSupportedCodecs to check if a
  // given codec id is supported by Chromecast or not. So by default we should
  // return true by default to indicate we have no reasons to believe this codec
  // is unsupported. This will allow the rest of MimeUtil checks to proceed as
  // usual.
  return true;
}

}  // namespace

::media::IsCodecSupportedCB GetIsCodecSupportedOnChromecastCB() {
  return base::Bind(&IsCodecSupported);
}

// Converts ::media::VideoCodec to chromecast::media::VideoCodec. Any unknown or
// unsupported codec will be converted to chromecast::media::kCodecUnknown.
VideoCodec ToCastVideoCodec(const ::media::VideoCodec video_codec) {
  switch (video_codec) {
    case ::media::kCodecH264:
      return kCodecH264;
    case ::media::kCodecVP8:
      return kCodecVP8;
    case ::media::kCodecVP9:
      return kCodecVP9;
    case ::media::kCodecHEVC:
      return kCodecHEVC;
    default:
      LOG(ERROR) << "Unsupported video codec " << video_codec;
  }
  return kVideoCodecUnknown;
}

// Converts ::media::VideoCodecProfile to chromecast::media::VideoProfile.
VideoProfile ToCastVideoProfile(
    const ::media::VideoCodecProfile codec_profile) {
  switch (codec_profile) {
    case ::media::H264PROFILE_BASELINE:
      return kH264Baseline;
    case ::media::H264PROFILE_MAIN:
      return kH264Main;
    case ::media::H264PROFILE_EXTENDED:
      return kH264Extended;
    case ::media::H264PROFILE_HIGH:
      return kH264High;
    case ::media::H264PROFILE_HIGH10PROFILE:
      return kH264High10;
    case ::media::H264PROFILE_HIGH422PROFILE:
      return kH264High422;
    case ::media::H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return kH264High444Predictive;
    case ::media::H264PROFILE_SCALABLEBASELINE:
      return kH264ScalableBaseline;
    case ::media::H264PROFILE_SCALABLEHIGH:
      return kH264ScalableHigh;
    case ::media::H264PROFILE_STEREOHIGH:
      return kH264Stereohigh;
    case ::media::H264PROFILE_MULTIVIEWHIGH:
      return kH264MultiviewHigh;
    case ::media::HEVCPROFILE_MAIN:
      return kHEVCMain;
    case ::media::HEVCPROFILE_MAIN10:
      return kHEVCMain10;
    case ::media::HEVCPROFILE_MAIN_STILL_PICTURE:
      return kHEVCMainStillPicture;
    case ::media::VP8PROFILE_ANY:
      return kVP8ProfileAny;
    case ::media::VP9PROFILE_PROFILE0:
      return kVP9Profile0;
    case ::media::VP9PROFILE_PROFILE1:
      return kVP9Profile1;
    case ::media::VP9PROFILE_PROFILE2:
      return kVP9Profile2;
    case ::media::VP9PROFILE_PROFILE3:
      return kVP9Profile3;
    default:
      LOG(INFO) << "Unsupported video codec profile " << codec_profile;
  }
  return kVideoProfileUnknown;
}

}  // namespace media
}  // namespace chromecast
