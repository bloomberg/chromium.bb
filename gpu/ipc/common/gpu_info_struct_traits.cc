// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_info_struct_traits.h"

namespace mojo {

// static
bool StructTraits<gpu::mojom::GpuDevice, gpu::GPUInfo::GPUDevice>::Read(
    gpu::mojom::GpuDeviceDataView data,
    gpu::GPUInfo::GPUDevice* out) {
  out->vendor_id = data.vendor_id();
  out->device_id = data.device_id();
  out->active = data.active();
  return data.ReadVendorString(&out->vendor_string) &&
         data.ReadDeviceString(&out->device_string);
}

// static
gpu::mojom::CollectInfoResult
EnumTraits<gpu::mojom::CollectInfoResult, gpu::CollectInfoResult>::ToMojom(
    gpu::CollectInfoResult collect_info_result) {
  switch (collect_info_result) {
    case gpu::CollectInfoResult::kCollectInfoNone:
      return gpu::mojom::CollectInfoResult::kCollectInfoNone;
    case gpu::CollectInfoResult::kCollectInfoSuccess:
      return gpu::mojom::CollectInfoResult::kCollectInfoSuccess;
    case gpu::CollectInfoResult::kCollectInfoNonFatalFailure:
      return gpu::mojom::CollectInfoResult::kCollectInfoNonFatalFailure;
    case gpu::CollectInfoResult::kCollectInfoFatalFailure:
      return gpu::mojom::CollectInfoResult::kCollectInfoFatalFailure;
  }
  NOTREACHED() << "Invalid CollectInfoResult value:" << collect_info_result;
  return gpu::mojom::CollectInfoResult::kCollectInfoNone;
}

// static
bool EnumTraits<gpu::mojom::CollectInfoResult, gpu::CollectInfoResult>::
    FromMojom(gpu::mojom::CollectInfoResult input,
              gpu::CollectInfoResult* out) {
  switch (input) {
    case gpu::mojom::CollectInfoResult::kCollectInfoNone:
      *out = gpu::CollectInfoResult::kCollectInfoNone;
      return true;
    case gpu::mojom::CollectInfoResult::kCollectInfoSuccess:
      *out = gpu::CollectInfoResult::kCollectInfoSuccess;
      return true;
    case gpu::mojom::CollectInfoResult::kCollectInfoNonFatalFailure:
      *out = gpu::CollectInfoResult::kCollectInfoNonFatalFailure;
      return true;
    case gpu::mojom::CollectInfoResult::kCollectInfoFatalFailure:
      *out = gpu::CollectInfoResult::kCollectInfoFatalFailure;
      return true;
  }
  NOTREACHED() << "Invalid CollectInfoResult value:" << input;
  return false;
}

// static
gpu::mojom::VideoCodecProfile
EnumTraits<gpu::mojom::VideoCodecProfile, gpu::VideoCodecProfile>::ToMojom(
    gpu::VideoCodecProfile video_codec_profile) {
  switch (video_codec_profile) {
    case gpu::VideoCodecProfile::VIDEO_CODEC_PROFILE_UNKNOWN:
      return gpu::mojom::VideoCodecProfile::VIDEO_CODEC_PROFILE_UNKNOWN;
    case gpu::VideoCodecProfile::H264PROFILE_BASELINE:
      return gpu::mojom::VideoCodecProfile::H264PROFILE_BASELINE;
    case gpu::VideoCodecProfile::H264PROFILE_MAIN:
      return gpu::mojom::VideoCodecProfile::H264PROFILE_MAIN;
    case gpu::VideoCodecProfile::H264PROFILE_EXTENDED:
      return gpu::mojom::VideoCodecProfile::H264PROFILE_EXTENDED;
    case gpu::VideoCodecProfile::H264PROFILE_HIGH:
      return gpu::mojom::VideoCodecProfile::H264PROFILE_HIGH;
    case gpu::VideoCodecProfile::H264PROFILE_HIGH10PROFILE:
      return gpu::mojom::VideoCodecProfile::H264PROFILE_HIGH10PROFILE;
    case gpu::VideoCodecProfile::H264PROFILE_HIGH422PROFILE:
      return gpu::mojom::VideoCodecProfile::H264PROFILE_HIGH422PROFILE;
    case gpu::VideoCodecProfile::H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return gpu::mojom::VideoCodecProfile::
          H264PROFILE_HIGH444PREDICTIVEPROFILE;
    case gpu::VideoCodecProfile::H264PROFILE_SCALABLEBASELINE:
      return gpu::mojom::VideoCodecProfile::H264PROFILE_SCALABLEBASELINE;
    case gpu::VideoCodecProfile::H264PROFILE_SCALABLEHIGH:
      return gpu::mojom::VideoCodecProfile::H264PROFILE_SCALABLEHIGH;
    case gpu::VideoCodecProfile::H264PROFILE_STEREOHIGH:
      return gpu::mojom::VideoCodecProfile::H264PROFILE_STEREOHIGH;
    case gpu::VideoCodecProfile::H264PROFILE_MULTIVIEWHIGH:
      return gpu::mojom::VideoCodecProfile::H264PROFILE_MULTIVIEWHIGH;
    case gpu::VideoCodecProfile::VP8PROFILE_ANY:
      return gpu::mojom::VideoCodecProfile::VP8PROFILE_ANY;
    case gpu::VideoCodecProfile::VP9PROFILE_PROFILE0:
      return gpu::mojom::VideoCodecProfile::VP9PROFILE_PROFILE0;
    case gpu::VideoCodecProfile::VP9PROFILE_PROFILE1:
      return gpu::mojom::VideoCodecProfile::VP9PROFILE_PROFILE1;
    case gpu::VideoCodecProfile::VP9PROFILE_PROFILE2:
      return gpu::mojom::VideoCodecProfile::VP9PROFILE_PROFILE2;
    case gpu::VideoCodecProfile::VP9PROFILE_PROFILE3:
      return gpu::mojom::VideoCodecProfile::VP9PROFILE_PROFILE3;
    case gpu::VideoCodecProfile::HEVCPROFILE_MAIN:
      return gpu::mojom::VideoCodecProfile::HEVCPROFILE_MAIN;
    case gpu::VideoCodecProfile::HEVCPROFILE_MAIN10:
      return gpu::mojom::VideoCodecProfile::HEVCPROFILE_MAIN10;
    case gpu::VideoCodecProfile::HEVCPROFILE_MAIN_STILL_PICTURE:
      return gpu::mojom::VideoCodecProfile::HEVCPROFILE_MAIN_STILL_PICTURE;
  }
  NOTREACHED() << "Invalid VideoCodecProfile:" << video_codec_profile;
  return gpu::mojom::VideoCodecProfile::VIDEO_CODEC_PROFILE_UNKNOWN;
}

// static
bool EnumTraits<gpu::mojom::VideoCodecProfile, gpu::VideoCodecProfile>::
    FromMojom(gpu::mojom::VideoCodecProfile input,
              gpu::VideoCodecProfile* out) {
  switch (input) {
    case gpu::mojom::VideoCodecProfile::VIDEO_CODEC_PROFILE_UNKNOWN:
      *out = gpu::VideoCodecProfile::VIDEO_CODEC_PROFILE_UNKNOWN;
      return true;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_BASELINE:
      *out = gpu::VideoCodecProfile::H264PROFILE_BASELINE;
      return true;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_MAIN:
      *out = gpu::VideoCodecProfile::H264PROFILE_MAIN;
      return true;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_EXTENDED:
      *out = gpu::VideoCodecProfile::H264PROFILE_EXTENDED;
      return true;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_HIGH:
      *out = gpu::VideoCodecProfile::H264PROFILE_HIGH;
      return true;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_HIGH10PROFILE:
      *out = gpu::VideoCodecProfile::H264PROFILE_HIGH10PROFILE;
      return true;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_HIGH422PROFILE:
      *out = gpu::VideoCodecProfile::H264PROFILE_HIGH422PROFILE;
      return true;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_HIGH444PREDICTIVEPROFILE:
      *out = gpu::VideoCodecProfile::H264PROFILE_HIGH444PREDICTIVEPROFILE;
      return true;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_SCALABLEBASELINE:
      *out = gpu::VideoCodecProfile::H264PROFILE_SCALABLEBASELINE;
      return true;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_SCALABLEHIGH:
      *out = gpu::VideoCodecProfile::H264PROFILE_SCALABLEHIGH;
      return true;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_STEREOHIGH:
      *out = gpu::VideoCodecProfile::H264PROFILE_STEREOHIGH;
      return true;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_MULTIVIEWHIGH:
      *out = gpu::VideoCodecProfile::H264PROFILE_MULTIVIEWHIGH;
      return true;
    case gpu::mojom::VideoCodecProfile::VP8PROFILE_ANY:
      *out = gpu::VideoCodecProfile::VP8PROFILE_ANY;
      return true;
    case gpu::mojom::VideoCodecProfile::VP9PROFILE_PROFILE0:
      *out = gpu::VideoCodecProfile::VP9PROFILE_PROFILE0;
      return true;
    case gpu::mojom::VideoCodecProfile::VP9PROFILE_PROFILE1:
      *out = gpu::VideoCodecProfile::VP9PROFILE_PROFILE1;
      return true;
    case gpu::mojom::VideoCodecProfile::VP9PROFILE_PROFILE2:
      *out = gpu::VideoCodecProfile::VP9PROFILE_PROFILE2;
      return true;
    case gpu::mojom::VideoCodecProfile::VP9PROFILE_PROFILE3:
      *out = gpu::VideoCodecProfile::VP9PROFILE_PROFILE3;
      return true;
    case gpu::mojom::VideoCodecProfile::HEVCPROFILE_MAIN:
      *out = gpu::VideoCodecProfile::HEVCPROFILE_MAIN;
      return true;
    case gpu::mojom::VideoCodecProfile::HEVCPROFILE_MAIN10:
      *out = gpu::VideoCodecProfile::HEVCPROFILE_MAIN10;
      return true;
    case gpu::mojom::VideoCodecProfile::HEVCPROFILE_MAIN_STILL_PICTURE:
      *out = gpu::VideoCodecProfile::HEVCPROFILE_MAIN_STILL_PICTURE;
      return true;
  }
  NOTREACHED() << "Invalid VideoCodecProfile: " << input;
  return false;
}

// static
bool StructTraits<gpu::mojom::VideoDecodeAcceleratorSupportedProfile,
                  gpu::VideoDecodeAcceleratorSupportedProfile>::
    Read(gpu::mojom::VideoDecodeAcceleratorSupportedProfileDataView data,
         gpu::VideoDecodeAcceleratorSupportedProfile* out) {
  out->encrypted_only = data.encrypted_only();
  return data.ReadProfile(&out->profile) &&
         data.ReadMaxResolution(&out->max_resolution) &&
         data.ReadMinResolution(&out->min_resolution);
}

// static
bool StructTraits<gpu::mojom::VideoDecodeAcceleratorCapabilities,
                  gpu::VideoDecodeAcceleratorCapabilities>::
    Read(gpu::mojom::VideoDecodeAcceleratorCapabilitiesDataView data,
         gpu::VideoDecodeAcceleratorCapabilities* out) {
  out->flags = data.flags();
  return true;
}

// static
bool StructTraits<gpu::mojom::VideoEncodeAcceleratorSupportedProfile,
                  gpu::VideoEncodeAcceleratorSupportedProfile>::
    Read(gpu::mojom::VideoEncodeAcceleratorSupportedProfileDataView data,
         gpu::VideoEncodeAcceleratorSupportedProfile* out) {
  out->max_framerate_numerator = data.max_framerate_numerator();
  out->max_framerate_denominator = data.max_framerate_denominator();
  return data.ReadProfile(&out->profile) &&
         data.ReadMaxResolution(&out->max_resolution);
}

}  // namespace mojo
