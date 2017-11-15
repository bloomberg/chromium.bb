// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/common/video_encode_accelerator_struct_traits.h"

namespace mojo {

// Make sure values in arc::mojom::VideoEncodeAccelerator::Error and
// media::VideoEncodeAccelerator::Error match.
#define CHECK_ERROR_ENUM(value)                                             \
  static_assert(                                                            \
      static_cast<int>(arc::mojom::VideoEncodeAccelerator::Error::value) == \
          media::VideoEncodeAccelerator::Error::value,                      \
      "enum ##value mismatch")

CHECK_ERROR_ENUM(kIllegalStateError);
CHECK_ERROR_ENUM(kInvalidArgumentError);
CHECK_ERROR_ENUM(kPlatformFailureError);
CHECK_ERROR_ENUM(kErrorMax);

#undef CHECK_ERROR_ENUM

// static
arc::mojom::VideoEncodeAccelerator::Error
EnumTraits<arc::mojom::VideoEncodeAccelerator::Error,
           media::VideoEncodeAccelerator::Error>::
    ToMojom(media::VideoEncodeAccelerator::Error input) {
  return static_cast<arc::mojom::VideoEncodeAccelerator::Error>(input);
}

// static
bool EnumTraits<arc::mojom::VideoEncodeAccelerator::Error,
                media::VideoEncodeAccelerator::Error>::
    FromMojom(arc::mojom::VideoEncodeAccelerator::Error input,
              media::VideoEncodeAccelerator::Error* output) {
  NOTIMPLEMENTED();
  return false;
}

// Make sure values in arc::mojom::VideoPixelFormat match to the values in
// media::VideoPixelFormat. The former is a subset of the later.
#define CHECK_PIXEL_FORMAT_ENUM(value)                                       \
  static_assert(                                                             \
      static_cast<int>(arc::mojom::VideoPixelFormat::value) == media::value, \
      "enum ##value mismatch")

CHECK_PIXEL_FORMAT_ENUM(PIXEL_FORMAT_I420);

#undef CHECK_PXIEL_FORMAT_ENUM

// static
arc::mojom::VideoPixelFormat
EnumTraits<arc::mojom::VideoPixelFormat, media::VideoPixelFormat>::ToMojom(
    media::VideoPixelFormat input) {
  NOTIMPLEMENTED();
  return arc::mojom::VideoPixelFormat::PIXEL_FORMAT_I420;
}

// static
bool EnumTraits<arc::mojom::VideoPixelFormat, media::VideoPixelFormat>::
    FromMojom(arc::mojom::VideoPixelFormat input,
              media::VideoPixelFormat* output) {
  switch (input) {
    case arc::mojom::VideoPixelFormat::PIXEL_FORMAT_I420:
      *output = static_cast<media::VideoPixelFormat>(input);
      return true;
    default:
      DLOG(ERROR) << "Unknown VideoPixelFormat: " << input;
      return false;
  }
}

// Make sure values in arc::mojom::VideoCodecProfile match to the values in
// media::VideoCodecProfile.
#define CHECK_PROFILE_ENUM(value)                                             \
  static_assert(                                                              \
      static_cast<int>(arc::mojom::VideoCodecProfile::value) == media::value, \
      "enum ##value mismatch")

CHECK_PROFILE_ENUM(VIDEO_CODEC_PROFILE_UNKNOWN);
CHECK_PROFILE_ENUM(VIDEO_CODEC_PROFILE_MIN);
CHECK_PROFILE_ENUM(H264PROFILE_MIN);
CHECK_PROFILE_ENUM(H264PROFILE_BASELINE);
CHECK_PROFILE_ENUM(H264PROFILE_MAIN);
CHECK_PROFILE_ENUM(H264PROFILE_EXTENDED);
CHECK_PROFILE_ENUM(H264PROFILE_HIGH);
CHECK_PROFILE_ENUM(H264PROFILE_HIGH10PROFILE);
CHECK_PROFILE_ENUM(H264PROFILE_HIGH422PROFILE);
CHECK_PROFILE_ENUM(H264PROFILE_HIGH444PREDICTIVEPROFILE);
CHECK_PROFILE_ENUM(H264PROFILE_SCALABLEBASELINE);
CHECK_PROFILE_ENUM(H264PROFILE_SCALABLEHIGH);
CHECK_PROFILE_ENUM(H264PROFILE_STEREOHIGH);
CHECK_PROFILE_ENUM(H264PROFILE_MULTIVIEWHIGH);
CHECK_PROFILE_ENUM(H264PROFILE_MAX);
CHECK_PROFILE_ENUM(VP8PROFILE_MIN);
CHECK_PROFILE_ENUM(VP8PROFILE_ANY);
CHECK_PROFILE_ENUM(VP8PROFILE_MAX);
CHECK_PROFILE_ENUM(VP9PROFILE_MIN);
CHECK_PROFILE_ENUM(VP9PROFILE_PROFILE0);
CHECK_PROFILE_ENUM(VP9PROFILE_PROFILE1);
CHECK_PROFILE_ENUM(VP9PROFILE_PROFILE2);
CHECK_PROFILE_ENUM(VP9PROFILE_PROFILE3);
CHECK_PROFILE_ENUM(VP9PROFILE_MAX);
CHECK_PROFILE_ENUM(HEVCPROFILE_MIN);
CHECK_PROFILE_ENUM(HEVCPROFILE_MAIN);
CHECK_PROFILE_ENUM(HEVCPROFILE_MAIN10);
CHECK_PROFILE_ENUM(HEVCPROFILE_MAIN_STILL_PICTURE);
CHECK_PROFILE_ENUM(HEVCPROFILE_MAX);
CHECK_PROFILE_ENUM(DOLBYVISION_MIN);
CHECK_PROFILE_ENUM(DOLBYVISION_PROFILE0);
CHECK_PROFILE_ENUM(DOLBYVISION_PROFILE4);
CHECK_PROFILE_ENUM(DOLBYVISION_PROFILE5);
CHECK_PROFILE_ENUM(DOLBYVISION_PROFILE7);
CHECK_PROFILE_ENUM(DOLBYVISION_MAX);
CHECK_PROFILE_ENUM(THEORAPROFILE_MIN);
CHECK_PROFILE_ENUM(THEORAPROFILE_ANY);
CHECK_PROFILE_ENUM(THEORAPROFILE_MAX);
CHECK_PROFILE_ENUM(AV1PROFILE_PROFILE0);
CHECK_PROFILE_ENUM(VIDEO_CODEC_PROFILE_MAX);

#undef CHECK_PROFILE_ENUM

// static
arc::mojom::VideoCodecProfile
EnumTraits<arc::mojom::VideoCodecProfile, media::VideoCodecProfile>::ToMojom(
    media::VideoCodecProfile input) {
  return static_cast<arc::mojom::VideoCodecProfile>(input);
}

// static
bool EnumTraits<arc::mojom::VideoCodecProfile, media::VideoCodecProfile>::
    FromMojom(arc::mojom::VideoCodecProfile input,
              media::VideoCodecProfile* output) {
  switch (input) {
    case arc::mojom::VideoCodecProfile::VIDEO_CODEC_PROFILE_UNKNOWN:
    case arc::mojom::VideoCodecProfile::H264PROFILE_BASELINE:
    case arc::mojom::VideoCodecProfile::H264PROFILE_MAIN:
    case arc::mojom::VideoCodecProfile::H264PROFILE_EXTENDED:
    case arc::mojom::VideoCodecProfile::H264PROFILE_HIGH:
    case arc::mojom::VideoCodecProfile::H264PROFILE_HIGH10PROFILE:
    case arc::mojom::VideoCodecProfile::H264PROFILE_HIGH422PROFILE:
    case arc::mojom::VideoCodecProfile::H264PROFILE_HIGH444PREDICTIVEPROFILE:
    case arc::mojom::VideoCodecProfile::H264PROFILE_SCALABLEBASELINE:
    case arc::mojom::VideoCodecProfile::H264PROFILE_SCALABLEHIGH:
    case arc::mojom::VideoCodecProfile::H264PROFILE_STEREOHIGH:
    case arc::mojom::VideoCodecProfile::H264PROFILE_MULTIVIEWHIGH:
    case arc::mojom::VideoCodecProfile::VP8PROFILE_ANY:
    case arc::mojom::VideoCodecProfile::VP9PROFILE_PROFILE0:
    case arc::mojom::VideoCodecProfile::VP9PROFILE_PROFILE1:
    case arc::mojom::VideoCodecProfile::VP9PROFILE_PROFILE2:
    case arc::mojom::VideoCodecProfile::VP9PROFILE_PROFILE3:
    case arc::mojom::VideoCodecProfile::HEVCPROFILE_MAIN:
    case arc::mojom::VideoCodecProfile::HEVCPROFILE_MAIN10:
    case arc::mojom::VideoCodecProfile::HEVCPROFILE_MAIN_STILL_PICTURE:
    case arc::mojom::VideoCodecProfile::DOLBYVISION_PROFILE0:
    case arc::mojom::VideoCodecProfile::DOLBYVISION_PROFILE4:
    case arc::mojom::VideoCodecProfile::DOLBYVISION_PROFILE5:
    case arc::mojom::VideoCodecProfile::DOLBYVISION_PROFILE7:
    case arc::mojom::VideoCodecProfile::THEORAPROFILE_ANY:
    case arc::mojom::VideoCodecProfile::AV1PROFILE_PROFILE0:
      *output = static_cast<media::VideoCodecProfile>(input);
      return true;
  }
  DLOG(ERROR) << "unknown profile: " << input;
  return false;
}

}  // namespace mojo
