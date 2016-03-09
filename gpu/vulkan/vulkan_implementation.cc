// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_implementation.h"

#include <string>
#include <vector>
#include <vulkan/vulkan.h>

#include "base/macros.h"

#if defined(VK_USE_PLATFORM_XLIB_KHR)
#include "ui/gfx/x/x11_types.h"
#endif  // defined(VK_USE_PLATFORM_XLIB_KHR)

namespace gpu {

struct VulkanInstance {
  VulkanInstance() : valid(false) {}

  void Initialize() {
    valid = InitializeVulkanInstance() && InitializeVulkanDevice();
  }

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

  bool InitializeVulkanDevice() {
    VkResult status = VK_SUCCESS;

    uint32_t device_count = 0;
    status = vkEnumeratePhysicalDevices(vk_instance, &device_count, nullptr);
    if (VK_SUCCESS != status || device_count == 0)
      return false;

    std::vector<VkPhysicalDevice> devices(device_count);
    status =
        vkEnumeratePhysicalDevices(vk_instance, &device_count, devices.data());
    if (VK_SUCCESS != status) {
      LOG(ERROR) << "vkEnumeratePhysicalDevices() failed: " << status;
      return false;
    }

// TODO(dyen): Enable this once vkGetPhysicalDeviceXlibPresentationSupportKHR()
// is properly supported in the driver.
#if 0 && defined(VK_USE_PLATFORM_XLIB_KHR)
    Display* xdisplay = gfx::GetXDisplay();
    VisualID visual_id =
        XVisualIDFromVisual(DefaultVisual(xdisplay, DefaultScreen(xdisplay)));
#endif  // defined(VK_USE_PLATFORM_XLIB_KHR)

    int device_index = -1;
    int queue_index = -1;
    for (size_t i = 0; i < devices.size(); ++i) {
      const VkPhysicalDevice& device = devices[i];
      uint32_t queue_count = 0;
      vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_count, nullptr);
      if (queue_count) {
        std::vector<VkQueueFamilyProperties> queue_properties(queue_count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_count,
                                                 queue_properties.data());
        for (size_t n = 0; n < queue_properties.size(); ++n) {
          if ((queue_properties[n].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
            continue;

// TODO(dyen): Enable this once vkGetPhysicalDeviceXlibPresentationSupportKHR()
// is properly supported in the driver.
#if 1
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
          if (!vkGetPhysicalDeviceXlibPresentationSupportKHR(
                  device, n, xdisplay, visual_id)) {
            continue;
          }
#else
#error Non-Supported Vulkan implementation.
#endif

          queue_index = static_cast<int>(n);
          break;
        }

        if (-1 != queue_index) {
          device_index = static_cast<int>(i);
          break;
        }
      }
    }

    if (queue_index == -1)
      return false;

    float queue_priority = 0.0f;
    VkDeviceQueueCreateInfo queue_create_info = {};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = queue_index;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;

    const char* device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = 1;
    device_create_info.pQueueCreateInfos = &queue_create_info;
    device_create_info.enabledExtensionCount = arraysize(device_extensions);
    device_create_info.ppEnabledExtensionNames = device_extensions;

    status = vkCreateDevice(devices[device_index], &device_create_info, nullptr,
                            &vk_device);
    if (VK_SUCCESS != status)
      return false;

    vkGetDeviceQueue(vk_device, queue_index, 0, &vk_queue);

    return true;
  }

  bool valid;
  VkInstance vk_instance;
  VkDevice vk_device;
  VkQueue vk_queue;
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

VkDevice GetVulkanDevice() {
  DCHECK(vulkan_instance);
  DCHECK(vulkan_instance->valid);
  return vulkan_instance->vk_device;
}

VkQueue GetVulkanQueue() {
  DCHECK(vulkan_instance);
  DCHECK(vulkan_instance->valid);
  return vulkan_instance->vk_queue;
}

}  // namespace gpu
