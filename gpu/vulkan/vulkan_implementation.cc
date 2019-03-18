// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_implementation.h"

#include "base/bind.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_instance.h"

namespace gpu {

VulkanImplementation::VulkanImplementation() {}

VulkanImplementation::~VulkanImplementation() {}

std::unique_ptr<VulkanDeviceQueue> CreateVulkanDeviceQueue(
    VulkanImplementation* vulkan_implementation,
    uint32_t option) {
  auto device_queue = std::make_unique<VulkanDeviceQueue>(
      vulkan_implementation->GetVulkanInstance()->vk_instance());
  auto callback = base::BindRepeating(
      &VulkanImplementation::GetPhysicalDevicePresentationSupport,
      base::Unretained(vulkan_implementation));
  std::vector<const char*> required_extensions =
      vulkan_implementation->GetRequiredDeviceExtensions();
  if (!device_queue->Initialize(option, std::move(required_extensions),
                                callback)) {
    device_queue->Destroy();
    return nullptr;
  }

  return device_queue;
}

bool VulkanImplementation::SubmitSignalSemaphore(VkQueue vk_queue,
                                                 VkSemaphore vk_semaphore,
                                                 VkFence vk_fence) {
  // Structure specifying a queue submit operation.
  VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &vk_semaphore;
  const unsigned int submit_count = 1;
  if (vkQueueSubmit(vk_queue, submit_count, &submit_info, vk_fence) !=
      VK_SUCCESS) {
    return false;
  }
  return true;
}

bool VulkanImplementation::SubmitWaitSemaphores(
    VkQueue vk_queue,
    const std::vector<VkSemaphore>& vk_semaphores,
    VkFence vk_fence) {
  DCHECK(!vk_semaphores.empty());
  // Structure specifying a queue submit operation.
  VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submit_info.waitSemaphoreCount = vk_semaphores.size();
  submit_info.pWaitSemaphores = vk_semaphores.data();
  const unsigned int submit_count = 1;
  if (vkQueueSubmit(vk_queue, submit_count, &submit_info, vk_fence) !=
      VK_SUCCESS) {
    return false;
  }
  return true;
}

#if defined(OS_LINUX) || defined(OS_ANDROID)
bool VulkanImplementation::ImportSemaphoreFdKHR(VkDevice vk_device,
                                                base::ScopedFD sync_fd,
                                                VkSemaphore* vk_semaphore) {
  if (!sync_fd.is_valid())
    return false;

  VkSemaphore semaphore = VK_NULL_HANDLE;
  VkSemaphoreCreateInfo info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  VkResult result = vkCreateSemaphore(vk_device, &info, nullptr, &semaphore);
  if (result != VK_SUCCESS)
    return false;

  VkImportSemaphoreFdInfoKHR import = {
      VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR};
  import.semaphore = semaphore;
#if defined(OS_ANDROID)
  import.flags = VK_SEMAPHORE_IMPORT_TEMPORARY_BIT_KHR;
  // VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT specifies a POSIX file
  // descriptor handle to a Linux Sync File or Android Fence object.
  import.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
#else
  import.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif
  import.fd = sync_fd.get();

  result = vkImportSemaphoreFdKHR(vk_device, &import);
  if (result != VK_SUCCESS) {
    vkDestroySemaphore(vk_device, semaphore, nullptr);
    return false;
  }

  // If import is successful, the VkSemaphore object takes the ownership of fd.
  ignore_result(sync_fd.release());
  *vk_semaphore = semaphore;
  return true;
}

bool VulkanImplementation::GetSemaphoreFdKHR(VkDevice vk_device,
                                             VkSemaphore vk_semaphore,
                                             base::ScopedFD* sync_fd) {
  // Create VkSemaphoreGetFdInfoKHR structure.
  VkSemaphoreGetFdInfoKHR info = {VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR};
  info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
  info.semaphore = vk_semaphore;
#if defined(OS_ANDROID)
  // VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT specifies a POSIX file
  // descriptor handle to a Linux Sync File or Android Fence object.
  info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
#else
  info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif

  // Create a new sync fd from the semaphore.
  int fd = -1;
  VkResult result = vkGetSemaphoreFdKHR(vk_device, &info, &fd);
  if (result != VK_SUCCESS) {
    LOG(ERROR) << "vkGetSemaphoreFdKHR failed : " << result;
    sync_fd->reset(-1);
    return false;
  }

  // Transfer the ownership of the fd to the caller.
  sync_fd->reset(fd);
  return true;
}
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

}  // namespace gpu
