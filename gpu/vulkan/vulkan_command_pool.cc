// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_command_pool.h"

#include "base/logging.h"
#include "gpu/vulkan/vulkan_command_buffer.h"
#include "gpu/vulkan/vulkan_implementation.h"

namespace gpu {

VulkanCommandPool::VulkanCommandPool(VkDevice device, uint32_t queue_index)
    : device_(device), queue_index_(queue_index) {}

VulkanCommandPool::~VulkanCommandPool() {
  DCHECK_EQ(0u, command_buffer_count_);
  DCHECK_EQ(static_cast<VkCommandPool>(VK_NULL_HANDLE), handle_);
}

bool VulkanCommandPool::Initialize() {
  VkCommandPoolCreateInfo command_pool_create_info = {};
  command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  command_pool_create_info.flags =
      VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  command_pool_create_info.queueFamilyIndex = queue_index_;

  VkResult result = vkCreateCommandPool(device_, &command_pool_create_info,
                                        nullptr, &handle_);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkCreateCommandPool() failed: " << result;
    return false;
  }

  return true;
}

void VulkanCommandPool::Destroy() {
  DCHECK_EQ(0u, command_buffer_count_);
  if (VK_NULL_HANDLE != handle_) {
    vkDestroyCommandPool(device_, handle_, nullptr);
    handle_ = VK_NULL_HANDLE;
  }
}

scoped_ptr<VulkanCommandBuffer>
VulkanCommandPool::CreatePrimaryCommandBuffer() {
  scoped_ptr<VulkanCommandBuffer> command_buffer(
      new VulkanCommandBuffer(this, true));
  if (!command_buffer->Initialize())
    return nullptr;

  return command_buffer;
}

scoped_ptr<VulkanCommandBuffer>
VulkanCommandPool::CreateSecondaryCommandBuffer() {
  scoped_ptr<VulkanCommandBuffer> command_buffer(
      new VulkanCommandBuffer(this, false));
  if (!command_buffer->Initialize())
    return nullptr;

  return command_buffer;
}

void VulkanCommandPool::IncrementCommandBufferCount() {
  command_buffer_count_++;
}

void VulkanCommandPool::DecrementCommandBufferCount() {
  DCHECK_LT(0u, command_buffer_count_);
  command_buffer_count_--;
}

}  // namespace gpu
