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

VkSemaphore VulkanImplementation::CreateExternalSemaphore(
    VkDevice vk_device,
    VkExternalSemaphoreHandleTypeFlags handle_types) {
  VkExportSemaphoreCreateInfo export_info = {
      VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO};
  export_info.handleTypes = handle_types;

  VkSemaphoreCreateInfo sem_info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                                    &export_info};

  VkSemaphore semaphore = VK_NULL_HANDLE;
  VkResult result =
      vkCreateSemaphore(vk_device, &sem_info, nullptr, &semaphore);

  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "Failed to create VkSemaphore: " << result;
    return VK_NULL_HANDLE;
  }

  return semaphore;
}

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

}  // namespace gpu
