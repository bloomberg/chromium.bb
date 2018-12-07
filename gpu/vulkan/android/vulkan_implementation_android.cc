// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/android/vulkan_implementation_android.h"

#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "ui/gfx/gpu_fence.h"

namespace gpu {

VulkanImplementationAndroid::VulkanImplementationAndroid() = default;

VulkanImplementationAndroid::~VulkanImplementationAndroid() = default;

bool VulkanImplementationAndroid::InitializeVulkanInstance() {
  std::vector<const char*> required_extensions = {
      VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
      VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
      VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
      VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME};

  VulkanFunctionPointers* vulkan_function_pointers =
      gpu::GetVulkanFunctionPointers();

  base::NativeLibraryLoadError native_library_load_error;
  vulkan_function_pointers->vulkan_loader_library_ = base::LoadNativeLibrary(
      base::FilePath("libvulkan.so"), &native_library_load_error);
  if (!vulkan_function_pointers->vulkan_loader_library_)
    return false;

  if (!vulkan_instance_.Initialize(required_extensions, {})) {
    vulkan_instance_.Destroy();
    return false;
  }

  // Initialize platform function pointers
  vkCreateAndroidSurfaceKHR_ =
      reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(vkGetInstanceProcAddr(
          vulkan_instance_.vk_instance(), "vkCreateAndroidSurfaceKHR"));
  if (!vkCreateAndroidSurfaceKHR_) {
    LOG(ERROR) << "vkCreateAndroidSurfaceKHR not found";
    vulkan_instance_.Destroy();
    return false;
  }

  return true;
}

VkInstance VulkanImplementationAndroid::GetVulkanInstance() {
  return vulkan_instance_.vk_instance();
}

std::unique_ptr<VulkanSurface> VulkanImplementationAndroid::CreateViewSurface(
    gfx::AcceleratedWidget window) {
  VkSurfaceKHR surface;
  VkAndroidSurfaceCreateInfoKHR surface_create_info = {};
  surface_create_info.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
  surface_create_info.window = window;
  VkResult result = vkCreateAndroidSurfaceKHR_(
      GetVulkanInstance(), &surface_create_info, nullptr, &surface);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkCreateAndroidSurfaceKHR() failed: " << result;
    return nullptr;
  }

  return std::make_unique<VulkanSurface>(GetVulkanInstance(), surface,
                                         base::DoNothing());
}

bool VulkanImplementationAndroid::GetPhysicalDevicePresentationSupport(
    VkPhysicalDevice device,
    const std::vector<VkQueueFamilyProperties>& queue_family_properties,
    uint32_t queue_family_index) {
  // On Android, all physical devices and queue families must be capable of
  // presentation with any native window.
  // As a result there is no Android-specific query for these capabilities.
  return true;
}

std::vector<const char*>
VulkanImplementationAndroid::GetRequiredDeviceExtensions() {
  return {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
          VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
          VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME};
}

VkFence VulkanImplementationAndroid::CreateVkFenceForGpuFence(
    VkDevice vk_device) {
  NOTREACHED();
  return VK_NULL_HANDLE;
}

std::unique_ptr<gfx::GpuFence>
VulkanImplementationAndroid::ExportVkFenceToGpuFence(VkDevice vk_device,
                                                     VkFence vk_fence) {
  NOTREACHED();
  return nullptr;
}

bool VulkanImplementationAndroid::ImportSemaphoreFdKHR(
    VkDevice vk_device,
    base::ScopedFD sync_fd,
    VkSemaphore* vk_semaphore) {
  // Create a VkSemaphore.
  VkSemaphoreCreateInfo info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  bool result = vkCreateSemaphore(vk_device, &info, nullptr, vk_semaphore);
  if (result != VK_SUCCESS) {
    LOG(ERROR) << "vkCreateSemaphore failed : " << result;
    return false;
  }

  // Create VkImportSemaphoreFdInfoKHR structure.
  VkImportSemaphoreFdInfoKHR import;
  import.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR;
  import.pNext = nullptr;
  import.semaphore = *vk_semaphore;
  import.flags = VK_SEMAPHORE_IMPORT_TEMPORARY_BIT_KHR;

  // VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT specifies a POSIX file
  // descriptor handle to a Linux Sync File or Android Fence object.
  import.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
  import.fd = sync_fd.get();

  // Import the fd into the semaphore.
  result = vkImportSemaphoreFdKHR(vk_device, &import);
  if (result != VK_SUCCESS) {
    LOG(ERROR) << "vkImportSemaphoreFdKHR failed : " << result;
    vkDestroySemaphore(vk_device, *vk_semaphore, nullptr);
    return false;
  }

  // If import is successful, the VkSemaphore object takes the ownership of fd.
  ignore_result(sync_fd.release());
  return true;
}

bool VulkanImplementationAndroid::GetSemaphoreFdKHR(VkDevice vk_device,
                                                    VkSemaphore vk_semaphore,
                                                    base::ScopedFD* sync_fd) {
  // Create VkSemaphoreGetFdInfoKHR structure.
  VkSemaphoreGetFdInfoKHR info;
  info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
  info.pNext = nullptr;
  info.semaphore = vk_semaphore;
  info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;

  // Create a new sync fd from the semaphore.
  int fd = -1;
  bool result = vkGetSemaphoreFdKHR(vk_device, &info, &fd);
  if (result != VK_SUCCESS) {
    LOG(ERROR) << "vkGetSemaphoreFdKHR failed : " << result;
    sync_fd->reset(-1);
    return false;
  }

  // Transfer the ownership of the fd to the caller.
  sync_fd->reset(fd);
  return true;
}

}  // namespace gpu
