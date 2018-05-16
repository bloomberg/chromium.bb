// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/init/vulkan_factory.h"
#include "build/build_config.h"

#include <memory>

#if defined(OS_ANDROID)
#include "gpu/vulkan/vulkan_implementation_android.h"
#endif

#if defined(USE_X11)
#include "gpu/vulkan/vulkan_implementation_x11.h"
#endif

namespace gpu {

std::unique_ptr<VulkanImplementation> CreateVulkanImplementation() {
#if defined(USE_X11)
  return std::make_unique<VulkanImplementationX11>();
#elif defined(OS_ANDROID)
  return std::make_unique<VulkanImplementationAndroid>();
#else
#error Unsupported Vulkan Platform.
#endif
}

}  // namespace gpu
