// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_swap_chain.h"

#include "base/bind.h"
#include "gpu/vulkan/vulkan_command_buffer.h"
#include "gpu/vulkan/vulkan_command_pool.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"

namespace gpu {

namespace {

VkPipelineStageFlags GetPipelineStageFlags(const VkImageLayout layout) {
  switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
      return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    case VK_IMAGE_LAYOUT_GENERAL:
      return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    case VK_IMAGE_LAYOUT_PREINITIALIZED:
      return VK_PIPELINE_STAGE_HOST_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      return VK_PIPELINE_STAGE_TRANSFER_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      return VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
             VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT |
             VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT |
             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    default:
      NOTREACHED() << "layout=" << layout;
  }
  return 0;
}

VkAccessFlags GetAccessMask(const VkImageLayout layout) {
  switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
      return 0;
    case VK_IMAGE_LAYOUT_GENERAL:
      DLOG(WARNING) << "VK_IMAGE_LAYOUT_GENERAL is used.";
      return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
             VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT |
             VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_HOST_WRITE_BIT |
             VK_ACCESS_HOST_READ_BIT;
    case VK_IMAGE_LAYOUT_PREINITIALIZED:
      return VK_ACCESS_HOST_WRITE_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      return VK_ACCESS_TRANSFER_READ_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      return VK_ACCESS_TRANSFER_WRITE_BIT;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      return 0;
    default:
      NOTREACHED() << "layout=" << layout;
  }
  return 0;
}

void CmdSetImageLayout(VulkanCommandBuffer* command_buffer,
                       VkImage image,
                       VkImageLayout layout,
                       VkImageLayout old_layout) {
  DCHECK_NE(layout, old_layout);
  VkImageMemoryBarrier image_memory_barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .pNext = nullptr,
      .srcAccessMask = GetAccessMask(old_layout),
      .dstAccessMask = GetAccessMask(layout),
      .oldLayout = old_layout,
      .newLayout = layout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange =
          {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
  };

  ScopedSingleUseCommandBufferRecorder recorder(*command_buffer);
  vkCmdPipelineBarrier(recorder.handle(), GetPipelineStageFlags(old_layout),
                       GetPipelineStageFlags(layout), 0, 0, nullptr, 0, nullptr,
                       1, &image_memory_barrier);
}

VkSemaphore CreateSemaphore(VkDevice vk_device) {
  // Generic semaphore creation structure.
  VkSemaphoreCreateInfo semaphore_create_info = {
      VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

  VkSemaphore vk_semaphore;
  auto result = vkCreateSemaphore(vk_device, &semaphore_create_info, nullptr,
                                  &vk_semaphore);
  DLOG_IF(FATAL, VK_SUCCESS != result)
      << "vkCreateSemaphore() failed: " << result;
  return vk_semaphore;
}

}  // namespace

VulkanSwapChain::VulkanSwapChain() {}

VulkanSwapChain::~VulkanSwapChain() {
  DCHECK(images_.empty());
  DCHECK_EQ(static_cast<VkSwapchainKHR>(VK_NULL_HANDLE), swap_chain_);
}

bool VulkanSwapChain::Initialize(
    VulkanDeviceQueue* device_queue,
    VkSurfaceKHR surface,
    const VkSurfaceCapabilitiesKHR& surface_caps,
    const VkSurfaceFormatKHR& surface_format,
    std::unique_ptr<VulkanSwapChain> old_swap_chain) {
  DCHECK(device_queue);
  device_queue_ = device_queue;
  device_queue_->GetFenceHelper()->ProcessCleanupTasks();
  return InitializeSwapChain(surface, surface_caps, surface_format,
                             std::move(old_swap_chain)) &&
         InitializeSwapImages(surface_caps, surface_format);
}

void VulkanSwapChain::Destroy() {
  DCHECK(!is_writing_);
  DestroySwapImages();
  DestroySwapChain();
}

gfx::SwapResult VulkanSwapChain::SwapBuffers() {
  DCHECK(end_write_semaphore_ != VK_NULL_HANDLE);

  VkResult result = VK_SUCCESS;
  VkDevice device = device_queue_->GetVulkanDevice();
  VkQueue queue = device_queue_->GetVulkanQueue();
  auto* fence_helper = device_queue_->GetFenceHelper();

  auto& current_image_data = images_[current_image_];
  if (current_image_data.layout != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
    current_image_data.command_buffer->Clear();
    CmdSetImageLayout(current_image_data.command_buffer.get(),
                      current_image_data.image,
                      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR /* layout */,
                      current_image_data.layout /* old_layout */);
    current_image_data.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkSemaphore vk_semaphore = CreateSemaphore(device);
    // Submit our command_buffer for the current buffer. It sets the image
    // layout for presenting.
    if (!current_image_data.command_buffer->Submit(1, &end_write_semaphore_, 1,
                                                   &vk_semaphore)) {
      vkDestroySemaphore(device, vk_semaphore, nullptr /* pAllocator */);
      return gfx::SwapResult::SWAP_FAILED;
    }
    current_image_data.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    fence_helper->EnqueueSemaphoreCleanupForSubmittedWork(end_write_semaphore_);
    end_write_semaphore_ = vk_semaphore;
  }

  // Queue the present.
  VkPresentInfoKHR present_info = {};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &end_write_semaphore_;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &swap_chain_;
  present_info.pImageIndices = &current_image_;

  result = vkQueuePresentKHR(queue, &present_info);
  if (VK_SUCCESS != result) {
    return gfx::SwapResult::SWAP_FAILED;
  }
  fence_helper->EnqueueSemaphoreCleanupForSubmittedWork(end_write_semaphore_);
  end_write_semaphore_ = VK_NULL_HANDLE;

  VkSemaphore vk_semaphore = CreateSemaphore(device);
  uint32_t next_image = 0;
  // Acquire then next image.
  result = vkAcquireNextImageKHR(device, swap_chain_, UINT64_MAX, vk_semaphore,
                                 VK_NULL_HANDLE, &next_image);
  if (VK_SUCCESS != result) {
    vkDestroySemaphore(device, vk_semaphore, nullptr /* pAllocator */);
    DLOG(ERROR) << "vkAcquireNextImageKHR() failed: " << result;
    return gfx::SwapResult::SWAP_FAILED;
  }

  current_image_ = next_image;
  DCHECK(begin_write_semaphore_ == VK_NULL_HANDLE);
  begin_write_semaphore_ = vk_semaphore;
  return gfx::SwapResult::SWAP_ACK;
}

bool VulkanSwapChain::InitializeSwapChain(
    VkSurfaceKHR surface,
    const VkSurfaceCapabilitiesKHR& surface_caps,
    const VkSurfaceFormatKHR& surface_format,
    std::unique_ptr<VulkanSwapChain> old_swap_chain) {
  VkDevice device = device_queue_->GetVulkanDevice();
  VkResult result = VK_SUCCESS;

  VkSwapchainCreateInfoKHR swap_chain_create_info = {};
  swap_chain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swap_chain_create_info.surface = surface;
  swap_chain_create_info.minImageCount =
      std::max(3u, surface_caps.minImageCount);
  swap_chain_create_info.imageFormat = surface_format.format;
  swap_chain_create_info.imageColorSpace = surface_format.colorSpace;
  swap_chain_create_info.imageExtent = surface_caps.currentExtent;
  swap_chain_create_info.imageArrayLayers = 1;
  swap_chain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swap_chain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  swap_chain_create_info.preTransform = surface_caps.currentTransform;
  swap_chain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swap_chain_create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
  swap_chain_create_info.clipped = true;
  swap_chain_create_info.oldSwapchain =
      old_swap_chain ? old_swap_chain->swap_chain_ : VK_NULL_HANDLE;

  VkSwapchainKHR new_swap_chain = VK_NULL_HANDLE;
  result = vkCreateSwapchainKHR(device, &swap_chain_create_info, nullptr,
                                &new_swap_chain);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkCreateSwapchainKHR() failed: " << result;
    result = vkCreateSwapchainKHR(device, &swap_chain_create_info, nullptr,
                                  &new_swap_chain);
  }

  if (old_swap_chain) {
    old_swap_chain->Destroy();
    old_swap_chain = nullptr;
  }

  swap_chain_ = new_swap_chain;
  size_ = gfx::Size(swap_chain_create_info.imageExtent.width,
                    swap_chain_create_info.imageExtent.height);

  return true;
}

void VulkanSwapChain::DestroySwapChain() {
  if (swap_chain_ == VK_NULL_HANDLE)
    return;

  device_queue_->GetFenceHelper()->EnqueueCleanupTaskForSubmittedWork(
      base::BindOnce(
          [](VkSwapchainKHR swapchain, VulkanDeviceQueue* device_queue,
             bool /* is_lost */) {
            VkDevice device = device_queue->GetVulkanDevice();
            vkDestroySwapchainKHR(device, swapchain, nullptr /* pAllocator */);
          },
          swap_chain_));
  swap_chain_ = VK_NULL_HANDLE;
}

bool VulkanSwapChain::InitializeSwapImages(
    const VkSurfaceCapabilitiesKHR& surface_caps,
    const VkSurfaceFormatKHR& surface_format) {
  VkDevice device = device_queue_->GetVulkanDevice();
  VkResult result = VK_SUCCESS;

  uint32_t image_count = 0;
  result = vkGetSwapchainImagesKHR(device, swap_chain_, &image_count, nullptr);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkGetSwapchainImagesKHR(NULL) failed: " << result;
    return false;
  }

  std::vector<VkImage> images(image_count);
  result =
      vkGetSwapchainImagesKHR(device, swap_chain_, &image_count, images.data());
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkGetSwapchainImagesKHR(images) failed: " << result;
    return false;
  }

  command_pool_ = device_queue_->CreateCommandPool();
  if (!command_pool_)
    return false;

  images_.resize(image_count);
  for (uint32_t i = 0; i < image_count; ++i) {
    auto& image_data = images_[i];
    image_data.image = images[i];
    // Initialize the command buffer for this buffer data.
    image_data.command_buffer = command_pool_->CreatePrimaryCommandBuffer();
  }

  VkSemaphore vk_semaphore = CreateSemaphore(device);
  // Acquire the initial buffer.
  result = vkAcquireNextImageKHR(device, swap_chain_, UINT64_MAX, vk_semaphore,
                                 VK_NULL_HANDLE, &current_image_);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkAcquireNextImageKHR() failed: " << result;
    return false;
  }
  begin_write_semaphore_ = vk_semaphore;
  return true;
}

void VulkanSwapChain::DestroySwapImages() {
  auto* fence_helper = device_queue_->GetFenceHelper();
  fence_helper->EnqueueCleanupTaskForSubmittedWork(base::BindOnce(
      [](VkSemaphore begin_semaphore, VkSemaphore end_semaphore,
         std::vector<ImageData> images,
         std::unique_ptr<VulkanCommandPool> command_pool,
         VulkanDeviceQueue* device_queue, bool /* is_lost */) {
        auto* vk_device = device_queue->GetVulkanDevice();
        if (begin_semaphore)
          vkDestroySemaphore(vk_device, begin_semaphore,
                             nullptr /* pAllocator */);
        if (end_semaphore)
          vkDestroySemaphore(vk_device, end_semaphore,
                             nullptr /* pAllocator */);
        for (auto& image_data : images) {
          if (!image_data.command_buffer)
            continue;
          image_data.command_buffer->Destroy();
          image_data.command_buffer = nullptr;
        }
        command_pool->Destroy();
      },
      begin_write_semaphore_, end_write_semaphore_, std::move(images_),
      std::move(command_pool_)));
  begin_write_semaphore_ = VK_NULL_HANDLE;
  end_write_semaphore_ = VK_NULL_HANDLE;
  images_.clear();
}

void VulkanSwapChain::BeginWriteCurrentImage(VkImage* image,
                                             uint32_t* image_index,
                                             VkImageLayout* image_layout,
                                             VkSemaphore* semaphore) {
  DCHECK(image);
  DCHECK(image_index);
  DCHECK(image_layout);
  DCHECK(semaphore);
  DCHECK(!is_writing_);
  DCHECK(begin_write_semaphore_ != VK_NULL_HANDLE);
  DCHECK(end_write_semaphore_ == VK_NULL_HANDLE);

  auto& current_image_data = images_[current_image_];
  *image = current_image_data.image;
  *image_index = current_image_;
  *image_layout = current_image_data.layout;
  *semaphore = begin_write_semaphore_;
  begin_write_semaphore_ = VK_NULL_HANDLE;
  is_writing_ = true;
}

void VulkanSwapChain::EndWriteCurrentImage(VkImageLayout image_layout,
                                           VkSemaphore semaphore) {
  DCHECK(is_writing_);
  DCHECK(begin_write_semaphore_ == VK_NULL_HANDLE);
  DCHECK(end_write_semaphore_ == VK_NULL_HANDLE);

  auto& current_image_data = images_[current_image_];
  current_image_data.layout = image_layout;
  end_write_semaphore_ = semaphore;
  is_writing_ = false;
}

VulkanSwapChain::ScopedWrite::ScopedWrite(VulkanSwapChain* swap_chain)
    : swap_chain_(swap_chain) {
  swap_chain_->BeginWriteCurrentImage(&image_, &image_index_, &image_layout_,
                                      &begin_semaphore_);
}

VulkanSwapChain::ScopedWrite::~ScopedWrite() {
  DCHECK(begin_semaphore_ == VK_NULL_HANDLE);
  swap_chain_->EndWriteCurrentImage(image_layout_, end_semaphore_);
}

VkSemaphore VulkanSwapChain::ScopedWrite::TakeBeginSemaphore() {
  DCHECK(begin_semaphore_ != VK_NULL_HANDLE);
  VkSemaphore semaphore = begin_semaphore_;
  begin_semaphore_ = VK_NULL_HANDLE;
  return semaphore;
}

void VulkanSwapChain::ScopedWrite::SetEndSemaphore(VkSemaphore semaphore) {
  DCHECK(end_semaphore_ == VK_NULL_HANDLE);
  DCHECK(semaphore != VK_NULL_HANDLE);
  end_semaphore_ = semaphore;
}

VulkanSwapChain::ImageData::ImageData() = default;
VulkanSwapChain::ImageData::ImageData(ImageData&& other) = default;
VulkanSwapChain::ImageData::~ImageData() = default;
VulkanSwapChain::ImageData& VulkanSwapChain::ImageData::operator=(
    ImageData&& other) = default;

}  // namespace gpu
