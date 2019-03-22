// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VULKAN_IMPLEMENTATION_H_
#define GPU_VULKAN_VULKAN_IMPLEMENTATION_H_

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "gpu/vulkan/vulkan_export.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_POSIX)
#include "base/files/scoped_file.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/scoped_hardware_buffer_handle.h"
#include "ui/gfx/geometry/size.h"
#endif

namespace gfx {
class GpuFence;
}

namespace gpu {

class VulkanDeviceQueue;
class VulkanSurface;
class VulkanInstance;

// Base class which provides functions for creating vulkan objects for different
// platforms that use platform-specific extensions (e.g. for creation of
// VkSurfaceKHR objects). It also provides helper/utility functions.
class VULKAN_EXPORT VulkanImplementation {
 public:
  VulkanImplementation();

  virtual ~VulkanImplementation();

  // Initialize VulkanInstance. If using_surface, VK_KHR_surface instance
  // extension will be required when initialize VkInstance. This extension is
  // required for presenting with VkSwapchain API.
  virtual bool InitializeVulkanInstance(bool using_surface = true) = 0;

  virtual VulkanInstance* GetVulkanInstance() = 0;

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

  // Submits a semaphore to be signalled to the vulkan queue. Semaphore is
  // signalled once this submission is executed. vk_fence is an optional handle
  // to fence to be signaled once this submission completes execution.
  bool SubmitSignalSemaphore(VkQueue vk_queue,
                             VkSemaphore vk_semaphore,
                             VkFence vk_fence = VK_NULL_HANDLE);

  // Submits semaphores to be waited upon to the vulkan queue. Semaphores are
  // waited on before this submission is executed. vk_fence is an optional
  // handle to fence to be signaled once this submission completes execution.
  bool SubmitWaitSemaphores(VkQueue vk_queue,
                            const std::vector<VkSemaphore>& vk_semaphores,
                            VkFence vk_fence = VK_NULL_HANDLE);

  // Submits a semaphore to be waited upon to the vulkan queue. Semaphore is
  // waited on before this submission is executed. vk_fence is an optional
  // handle to fence to be signaled once this submission completes execution.
  bool SubmitWaitSemaphore(VkQueue vk_queue,
                           VkSemaphore vk_semaphore,
                           VkFence vk_fence = VK_NULL_HANDLE) {
    return SubmitWaitSemaphores(vk_queue, {vk_semaphore}, vk_fence);
  }

#if defined(OS_LINUX) || defined(OS_ANDROID)
  // Import a VkSemaphore from a POSIX sync file descriptor. Importing a
  // semaphore payload from a file descriptor transfers ownership of the file
  // descriptor from the application to the Vulkan implementation. The
  // application must not perform any operations on the file descriptor after a
  // successful import.
  virtual bool ImportSemaphoreFdKHR(VkDevice vk_device,
                                    base::ScopedFD sync_fd,
                                    VkSemaphore* vk_semaphore);

  // Export a sync fd representing the payload of a semaphore.
  virtual bool GetSemaphoreFdKHR(VkDevice vk_device,
                                 VkSemaphore vk_semaphore,
                                 base::ScopedFD* sync_fd);
#endif

#if defined(OS_ANDROID)
  // Create a VkImage, import Android AHardwareBuffer object created outside of
  // the Vulkan device into Vulkan memory object and bind it to the VkImage.
  virtual bool CreateVkImageAndImportAHB(
      const VkDevice& vk_device,
      const VkPhysicalDevice& vk_physical_device,
      const gfx::Size& size,
      base::android::ScopedHardwareBufferHandle ahb_handle,
      VkImage* vk_image,
      VkImageCreateInfo* vk_image_info,
      VkDeviceMemory* vk_device_memory,
      VkDeviceSize* mem_allocation_size) = 0;
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
