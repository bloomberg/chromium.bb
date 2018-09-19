// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/init/vulkan_factory.h"
#include "build/build_config.h"

#include <memory>

#if defined(OS_ANDROID)
#include "gpu/vulkan/android/vulkan_implementation_android.h"
#endif

#if defined(USE_X11)
#include "gpu/vulkan/x/vulkan_implementation_x11.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif

namespace gpu {

std::unique_ptr<VulkanImplementation> CreateVulkanImplementation() {
#if defined(USE_X11)
  return std::make_unique<VulkanImplementationX11>();
#elif defined(OS_ANDROID)
  return std::make_unique<VulkanImplementationAndroid>();
#elif defined(USE_OZONE)
  return ui::OzonePlatform::GetInstance()
      ->GetSurfaceFactoryOzone()
      ->CreateVulkanImplementation();
#else
#error Unsupported Vulkan Platform.
#endif
}

}  // namespace gpu
