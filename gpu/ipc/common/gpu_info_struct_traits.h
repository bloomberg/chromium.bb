// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_GPU_STRUCT_TRAITS_H_
#define CC_IPC_GPU_STRUCT_TRAITS_H_

#include "gpu/config/gpu_info.h"
#include "gpu/ipc/common/gpu_info.mojom.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"

namespace mojo {

template <>
struct StructTraits<gpu::mojom::GpuDevice, gpu::GPUInfo::GPUDevice> {
  static bool Read(gpu::mojom::GpuDeviceDataView data,
                   gpu::GPUInfo::GPUDevice* out);

  static uint32_t vendor_id(const gpu::GPUInfo::GPUDevice& input) {
    return input.vendor_id;
  }

  static uint32_t device_id(const gpu::GPUInfo::GPUDevice& input) {
    return input.device_id;
  }

  static bool active(const gpu::GPUInfo::GPUDevice& input) {
    return input.active;
  }

  static const std::string& vendor_string(
      const gpu::GPUInfo::GPUDevice& input) {
    return input.vendor_string;
  }

  static const std::string& device_string(
      const gpu::GPUInfo::GPUDevice& input) {
    return input.device_string;
  }
};

template <>
struct EnumTraits<gpu::mojom::CollectInfoResult, gpu::CollectInfoResult> {
  static gpu::mojom::CollectInfoResult ToMojom(
      gpu::CollectInfoResult collect_info_result);

  static bool FromMojom(gpu::mojom::CollectInfoResult input,
                        gpu::CollectInfoResult* out);
};

template <>
struct EnumTraits<gpu::mojom::VideoCodecProfile, gpu::VideoCodecProfile> {
  static gpu::mojom::VideoCodecProfile ToMojom(
      gpu::VideoCodecProfile video_codec_profile);
  static bool FromMojom(gpu::mojom::VideoCodecProfile input,
                        gpu::VideoCodecProfile* out);
};

template <>
struct StructTraits<gpu::mojom::VideoDecodeAcceleratorSupportedProfile,
                    gpu::VideoDecodeAcceleratorSupportedProfile> {
  static bool Read(
      gpu::mojom::VideoDecodeAcceleratorSupportedProfileDataView data,
      gpu::VideoDecodeAcceleratorSupportedProfile* out);

  static gpu::VideoCodecProfile profile(
      const gpu::VideoDecodeAcceleratorSupportedProfile& input) {
    return input.profile;
  }

  static const gfx::Size& max_resolution(
      const gpu::VideoDecodeAcceleratorSupportedProfile& input) {
    return input.max_resolution;
  }

  static const gfx::Size& min_resolution(
      const gpu::VideoDecodeAcceleratorSupportedProfile& input) {
    return input.min_resolution;
  }

  static bool encrypted_only(
      const gpu::VideoDecodeAcceleratorSupportedProfile& input) {
    return input.encrypted_only;
  }
};

template <>
struct StructTraits<gpu::mojom::VideoDecodeAcceleratorCapabilities,
                    gpu::VideoDecodeAcceleratorCapabilities> {
  static bool Read(gpu::mojom::VideoDecodeAcceleratorCapabilitiesDataView data,
                   gpu::VideoDecodeAcceleratorCapabilities* out);

  static uint32_t flags(const gpu::VideoDecodeAcceleratorCapabilities& input) {
    return input.flags;
  }
};

template <>
struct StructTraits<gpu::mojom::VideoEncodeAcceleratorSupportedProfile,
                    gpu::VideoEncodeAcceleratorSupportedProfile> {
  static bool Read(
      gpu::mojom::VideoEncodeAcceleratorSupportedProfileDataView data,
      gpu::VideoEncodeAcceleratorSupportedProfile* out);

  static gpu::VideoCodecProfile profile(
      const gpu::VideoEncodeAcceleratorSupportedProfile& input) {
    return input.profile;
  }

  static const gfx::Size& max_resolution(
      const gpu::VideoEncodeAcceleratorSupportedProfile& input) {
    return input.max_resolution;
  }

  static uint32_t max_framerate_numerator(
      const gpu::VideoEncodeAcceleratorSupportedProfile& input) {
    return input.max_framerate_numerator;
  }

  static uint32_t max_framerate_denominator(
      const gpu::VideoEncodeAcceleratorSupportedProfile& input) {
    return input.max_framerate_denominator;
  }
};

}  // namespace mojo
#endif  // CC_IPC_GPU_STRUCT_TRAITS_H_
