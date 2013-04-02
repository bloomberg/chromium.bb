// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_GPU_FEATURE_TYPE_H_
#define CONTENT_PUBLIC_COMMON_GPU_FEATURE_TYPE_H_

namespace content {

// Provides flags indicating which gpu features are blacklisted for the system
// on which chrome is currently running.
// If a bit is set to 1, corresponding feature is blacklisted.
enum GpuFeatureType {
  GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS = 0,
  GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING,
  GPU_FEATURE_TYPE_WEBGL,
  GPU_FEATURE_TYPE_MULTISAMPLING,
  GPU_FEATURE_TYPE_FLASH3D,
  GPU_FEATURE_TYPE_FLASH_STAGE3D,
  GPU_FEATURE_TYPE_TEXTURE_SHARING,
  GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
  GPU_FEATURE_TYPE_3D_CSS,
  GPU_FEATURE_TYPE_ACCELERATED_VIDEO,
  GPU_FEATURE_TYPE_PANEL_FITTING,
  GPU_FEATURE_TYPE_FORCE_COMPOSITING_MODE,
  GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
  NUMBER_OF_GPU_FEATURE_TYPES
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_GPU_FEATURE_TYPE_H_
