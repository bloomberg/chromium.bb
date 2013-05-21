// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_blacklist.h"

#include "content/public/common/gpu_feature_type.h"

namespace content {

GpuBlacklist::GpuBlacklist()
    : GpuControlList() {
}

GpuBlacklist::~GpuBlacklist() {
}

// static
GpuBlacklist* GpuBlacklist::Create() {
  GpuBlacklist* list = new GpuBlacklist();
  list->AddSupportedFeature("accelerated_2d_canvas",
                            GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS);
  list->AddSupportedFeature("accelerated_compositing",
                            GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING);
  list->AddSupportedFeature("webgl",
                            GPU_FEATURE_TYPE_WEBGL);
  list->AddSupportedFeature("multisampling",
                            GPU_FEATURE_TYPE_MULTISAMPLING);
  list->AddSupportedFeature("flash_3d",
                            GPU_FEATURE_TYPE_FLASH3D);
  list->AddSupportedFeature("flash_stage3d",
                            GPU_FEATURE_TYPE_FLASH_STAGE3D);
  list->AddSupportedFeature("flash_stage3d_baseline",
                            GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE);
  list->AddSupportedFeature("texture_sharing",
                            GPU_FEATURE_TYPE_TEXTURE_SHARING);
  list->AddSupportedFeature("accelerated_video_decode",
                            GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE);
  list->AddSupportedFeature("3d_css",
                            GPU_FEATURE_TYPE_3D_CSS);
  list->AddSupportedFeature("accelerated_video",
                            GPU_FEATURE_TYPE_ACCELERATED_VIDEO);
  list->AddSupportedFeature("panel_fitting",
                            GPU_FEATURE_TYPE_PANEL_FITTING);
  list->AddSupportedFeature("force_compositing_mode",
                            GPU_FEATURE_TYPE_FORCE_COMPOSITING_MODE);
  list->set_supports_feature_type_all(true);
  return list;
}

}  // namespace content
