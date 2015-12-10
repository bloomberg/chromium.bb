// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_GPU_VIDEO_ACCELERATOR_UTIL_H_
#define CONTENT_COMMON_GPU_MEDIA_GPU_VIDEO_ACCELERATOR_UTIL_H_

#include <vector>

#include "gpu/config/gpu_info.h"
#include "media/video/video_decode_accelerator.h"
#include "media/video/video_encode_accelerator.h"

namespace content {

class GpuVideoAcceleratorUtil {
 public:
  // Convert decoder gpu capabilities to media capabilities.
  static media::VideoDecodeAccelerator::Capabilities
  ConvertGpuToMediaDecodeCapabilities(
      const gpu::VideoDecodeAcceleratorCapabilities& gpu_capabilities);

  // Convert decoder gpu profiles to media profiles.
  static media::VideoDecodeAccelerator::SupportedProfiles
      ConvertGpuToMediaDecodeProfiles(const
          gpu::VideoDecodeAcceleratorSupportedProfiles& gpu_profiles);

  // Convert decoder media capabilities to gpu capabilities.
  static gpu::VideoDecodeAcceleratorCapabilities
  ConvertMediaToGpuDecodeCapabilities(
      const media::VideoDecodeAccelerator::Capabilities& media_capabilities);

  // Convert decoder media profiles to gpu profiles.
  static gpu::VideoDecodeAcceleratorSupportedProfiles
      ConvertMediaToGpuDecodeProfiles(const
          media::VideoDecodeAccelerator::SupportedProfiles& media_profiles);

  // Convert encoder gpu profiles to media profiles.
  static media::VideoEncodeAccelerator::SupportedProfiles
      ConvertGpuToMediaEncodeProfiles(const
          gpu::VideoEncodeAcceleratorSupportedProfiles& gpu_profiles);

  // Convert encoder media profiles to gpu profiles.
  static gpu::VideoEncodeAcceleratorSupportedProfiles
      ConvertMediaToGpuEncodeProfiles(const
          media::VideoEncodeAccelerator::SupportedProfiles& media_profiles);

  // Insert |new_profiles| into |media_profiles|, ensuring no duplicates are
  // inserted.
  static void InsertUniqueDecodeProfiles(
      const media::VideoDecodeAccelerator::SupportedProfiles& new_profiles,
      media::VideoDecodeAccelerator::SupportedProfiles* media_profiles);

  // Insert |new_profiles| into |media_profiles|, ensuring no duplicates are
  // inserted.
  static void InsertUniqueEncodeProfiles(
      const media::VideoEncodeAccelerator::SupportedProfiles& new_profiles,
      media::VideoEncodeAccelerator::SupportedProfiles* media_profiles);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_GPU_VIDEO_ACCELERATOR_UTIL_H_
