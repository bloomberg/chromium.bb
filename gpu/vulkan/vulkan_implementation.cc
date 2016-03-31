// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_implementation.h"

#include <vulkan/vulkan.h>

#include "base/logging.h"
#include "base/macros.h"

#if defined(VK_USE_PLATFORM_XLIB_KHR)
#include "ui/gfx/x/x11_types.h"
#endif  // defined(VK_USE_PLATFORM_XLIB_KHR)

namespace gpu {

struct VulkanInstance {
  VulkanInstance() {}

  void Initialize() { valid = InitializeVulkanInstance(); }

  bool InitializeVulkanInstance() {
    VkResult status = VK_SUCCESS;

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Chromium";
    app_info.apiVersion = VK_MAKE_VERSION(1, 0, 2);

    const char* instance_extensions[] = {
      VK_KHR_SURFACE_EXTENSION_NAME,

#if defined(VK_USE_PLATFORM_XLIB_KHR)
      VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#endif  // defined(VK_USE_PLATFORM_XLIB_KHR)
    };

    VkInstanceCreateInfo instance_create_info = {};
    instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create_info.pApplicationInfo = &app_info;
    instance_create_info.ppEnabledExtensionNames = instance_extensions;
    instance_create_info.enabledExtensionCount = arraysize(instance_extensions);

    status = vkCreateInstance(&instance_create_info, nullptr, &vk_instance);
    DCHECK_EQ(VK_SUCCESS, status);
    if (VK_SUCCESS != status)
      return false;

    return true;
  }

  bool valid = false;
  VkInstance vk_instance = VK_NULL_HANDLE;
};

static VulkanInstance* vulkan_instance = nullptr;

bool InitializeVulkan() {
  DCHECK(!vulkan_instance);
  vulkan_instance = new VulkanInstance;
  vulkan_instance->Initialize();
  return vulkan_instance->valid;
}

VkInstance GetVulkanInstance() {
  DCHECK(vulkan_instance);
  DCHECK(vulkan_instance->valid);
  return vulkan_instance->vk_instance;
}

}  // namespace gpu
