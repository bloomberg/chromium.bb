// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VULKAN_IMPLEMENTATION_H_
#define GPU_VULKAN_VULKAN_IMPLEMENTATION_H_

#include <vulkan/vulkan.h>
#include <memory>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "gpu/vulkan/vulkan_export.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class GpuFence;
}

namespace gpu {

class VulkanDeviceQueue;
class VulkanSurface;

// This object provides factory functions for creating vulkan objects that use
// platform-specific extensions (e.g. for creation of VkSurfaceKHR objects).
class VULKAN_EXPORT VulkanImplementation {
 public:
  VulkanImplementation();

  virtual ~VulkanImplementation();

  virtual bool InitializeVulkanInstance() = 0;

  virtual VkInstance GetVulkanInstance() = 0;

  virtual std::unique_ptr<VulkanSurface> CreateViewSurface(
      gfx::AcceleratedWidget window) = 0;

  virtual bool GetPhysicalDevicePresentationSupport(
      VkPhysicalDevice device,
      const std::vector<VkQueueFamilyProperties>& queue_family_properties,
      uint32_t queue_family_index) = 0;

  virtual std::vector<const char*> GetRequiredDeviceExtensions() = 0;

  // Creates a VkFence that is exportable to a gfx::GpuFence.
  virtual VkFence CreateVkFenceForGpuFence(VkDevice vk_device) = 0;

  // Exports a VkFence to a gfx::GpuFence.
  //
  // The fence should have been created via CreateVkFenceForGpuFence().
  virtual std::unique_ptr<gfx::GpuFence> ExportVkFenceToGpuFence(
      VkDevice vk_device,
      VkFence vk_fence) = 0;

#if defined(OS_ANDROID)
  // Import a VkSemaphore from a POSIX sync file descriptor. Importing a
  // semaphore payload from a file descriptor transfers ownership of the file
  // descriptor from the application to the Vulkan implementation. The
  // application must not perform any operations on the file descriptor after a
  // successful import.
  virtual bool ImportSemaphoreFdKHR(VkDevice vk_device,
                                    base::ScopedFD sync_fd,
                                    VkSemaphore* vk_semaphore) = 0;

  // Export a sync fd representing the payload of a semaphore.
  virtual bool GetSemaphoreFdKHR(VkDevice vk_device,
                                 VkSemaphore vk_semaphore,
                                 base::ScopedFD* sync_fd) = 0;
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(VulkanImplementation);
};

VULKAN_EXPORT
std::unique_ptr<VulkanDeviceQueue> CreateVulkanDeviceQueue(
    VulkanImplementation* vulkan_implementation,
    uint32_t option);

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_IMPLEMENTATION_H_
