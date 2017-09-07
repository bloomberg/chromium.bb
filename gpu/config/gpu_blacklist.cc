// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_blacklist.h"

#include "gpu/config/gpu_feature_type.h"
#include "gpu/config/software_rendering_list_autogen.h"

namespace gpu {

GpuBlacklist::GpuBlacklist(const GpuControlListData& data)
    : GpuControlList(data) {}

GpuBlacklist::~GpuBlacklist() {
}

// static
std::unique_ptr<GpuBlacklist> GpuBlacklist::Create() {
  GpuControlListData data(kSoftwareRenderingListVersion,
                          kSoftwareRenderingListEntryCount,
                          kSoftwareRenderingListEntries);
  return Create(data);
}

// static
std::unique_ptr<GpuBlacklist> GpuBlacklist::Create(
    const GpuControlListData& data) {
  std::unique_ptr<GpuBlacklist> list(new GpuBlacklist(data));
  list->AddSupportedFeature("accelerated_2d_canvas",
                            GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS);
  list->AddSupportedFeature("gpu_compositing",
                            GPU_FEATURE_TYPE_GPU_COMPOSITING);
  list->AddSupportedFeature("accelerated_webgl",
                            GPU_FEATURE_TYPE_ACCELERATED_WEBGL);
  list->AddSupportedFeature("flash3d", GPU_FEATURE_TYPE_FLASH3D);
  list->AddSupportedFeature("flash_stage3d", GPU_FEATURE_TYPE_FLASH_STAGE3D);
  list->AddSupportedFeature("flash_stage3d_baseline",
                            GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE);
  list->AddSupportedFeature("accelerated_video_decode",
                            GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE);
  list->AddSupportedFeature("accelerated_video_encode",
                            GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE);
  list->AddSupportedFeature("panel_fitting", GPU_FEATURE_TYPE_PANEL_FITTING);
  list->AddSupportedFeature("gpu_rasterization",
                            GPU_FEATURE_TYPE_GPU_RASTERIZATION);
  list->AddSupportedFeature("accelerated_webgl2",
                            GPU_FEATURE_TYPE_ACCELERATED_WEBGL2);
  return list;
}

}  // namespace gpu
