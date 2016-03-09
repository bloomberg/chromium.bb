// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_surface.h"

#include "gpu/vulkan/vulkan_implementation.h"

namespace gpu {

VulkanSurface::VulkanSurface() {}

// static
bool VulkanSurface::InitializeOneOff() {
  if (!InitializeVulkan())
    return false;

  return true;
}

VulkanSurface::~VulkanSurface() {}

}  // namespace gpu
