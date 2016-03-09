// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VULKAN_SURFACE_H_
#define GPU_VULKAN_VULKAN_SURFACE_H_

#include "gpu/vulkan/vulkan_export.h"

namespace gpu {

class VULKAN_EXPORT VulkanSurface {
 public:
  VulkanSurface();

  static bool InitializeOneOff();

 protected:
  virtual ~VulkanSurface();
};

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_SURFACE_H_
