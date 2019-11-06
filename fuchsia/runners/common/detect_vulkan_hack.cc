// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/common/detect_vulkan_hack.h"
#include "gpu/vulkan/vulkan_function_pointers.h"

// Removes the VULKAN feature flag if Vulkan is not available on the host.
void DisableVulkanIfUnavailable(fuchsia::web::ContextFeatureFlags* features) {
  gpu::VulkanFunctionPointers* vulkan_ptrs = gpu::GetVulkanFunctionPointers();
  if (!vulkan_ptrs->BindUnassociatedFunctionPointers())
    *features &= ~fuchsia::web::ContextFeatureFlags::VULKAN;
}
