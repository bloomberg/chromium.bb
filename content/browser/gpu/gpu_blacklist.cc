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
  list->AddFeature("accelerated_2d_canvas",
                   GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS);
  list->AddFeature("accelerated_compositing",
                   GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING);
  list->AddFeature("webgl",
                   GPU_FEATURE_TYPE_WEBGL);
  list->AddFeature("multisampling",
                   GPU_FEATURE_TYPE_MULTISAMPLING);
  list->AddFeature("flash_3d",
                   GPU_FEATURE_TYPE_FLASH3D);
  list->AddFeature("flash_stage3d",
                   GPU_FEATURE_TYPE_FLASH_STAGE3D);
  list->AddFeature("flash_stage3d_baseline",
                   GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE);
  list->AddFeature("texture_sharing",
                   GPU_FEATURE_TYPE_TEXTURE_SHARING);
  list->AddFeature("accelerated_video_decode",
                   GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE);
  list->AddFeature("3d_css",
                   GPU_FEATURE_TYPE_3D_CSS);
  list->AddFeature("accelerated_video",
                   GPU_FEATURE_TYPE_ACCELERATED_VIDEO);
  list->AddFeature("panel_fitting",
                   GPU_FEATURE_TYPE_PANEL_FITTING);
  list->AddFeature("force_compositing_mode",
                   GPU_FEATURE_TYPE_FORCE_COMPOSITING_MODE);
  list->AddFeature("all",
                   GPU_FEATURE_TYPE_ALL);
  return list;
}

}  // namespace content
