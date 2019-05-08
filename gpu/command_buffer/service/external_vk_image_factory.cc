// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/external_vk_image_factory.h"

#include <unistd.h>

#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/service/external_vk_image_backing.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/vulkan/vulkan_command_buffer.h"
#include "gpu/vulkan/vulkan_command_pool.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"

namespace gpu {

ExternalVkImageFactory::ExternalVkImageFactory(
    SharedContextState* context_state)
    : context_state_(context_state) {}

ExternalVkImageFactory::~ExternalVkImageFactory() {
  if (command_pool_) {
    context_state_->vk_context_provider()
        ->GetDeviceQueue()
        ->GetFenceHelper()
        ->EnqueueVulkanObjectCleanupForSubmittedWork(std::move(command_pool_));
  }
}

std::unique_ptr<SharedImageBacking> ExternalVkImageFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    bool is_thread_safe) {
  DCHECK(!is_thread_safe);
  VkDevice device = context_state_->vk_context_provider()
                        ->GetDeviceQueue()
                        ->GetVulkanDevice();

  VkFormat vk_format = ToVkFormat(format);
  VkImage image;
  VkResult result = CreateExternalVkImage(vk_format, size, &image);
  if (result != VK_SUCCESS) {
    LOG(ERROR) << "Failed to create external VkImage: " << result;
    return nullptr;
  }

  VkMemoryRequirements requirements;
  vkGetImageMemoryRequirements(device, image, &requirements);

  if (!requirements.memoryTypeBits) {
    LOG(ERROR) << "Unable to find appropriate memory type for external VkImage";
    vkDestroyImage(device, image, nullptr);
    return nullptr;
  }

  constexpr uint32_t kInvalidTypeIndex = 32;
  uint32_t type_index = kInvalidTypeIndex;
  for (int i = 0; i < 32; i++) {
    if ((1u << i) & requirements.memoryTypeBits) {
      type_index = i;
      break;
    }
  }
  DCHECK_NE(kInvalidTypeIndex, type_index);

  VkExportMemoryAllocateInfoKHR external_info;
  external_info.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR;
  external_info.pNext = nullptr;
  external_info.handleTypes = context_state_->vk_context_provider()
                                  ->GetVulkanImplementation()
                                  ->GetExternalImageHandleType();

  VkMemoryAllocateInfo mem_alloc_info;
  mem_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  mem_alloc_info.pNext = &external_info;
  mem_alloc_info.allocationSize = requirements.size;
  mem_alloc_info.memoryTypeIndex = type_index;

  VkDeviceMemory memory;
  // TODO(crbug.com/932286): Allocating a separate piece of memory for every
  // VkImage might have too much overhead. It is recommended that one large
  // VkDeviceMemory be sub-allocated to multiple VkImages instead.
  result = vkAllocateMemory(device, &mem_alloc_info, nullptr, &memory);
  if (result != VK_SUCCESS) {
    LOG(ERROR) << "Failed to allocate memory for external VkImage: " << result;
    vkDestroyImage(device, image, nullptr);
    return nullptr;
  }

  result = vkBindImageMemory(device, image, memory, 0);
  if (result != VK_SUCCESS) {
    LOG(ERROR) << "Failed to bind memory to external VkImage: " << result;
    vkFreeMemory(device, memory, nullptr);
    vkDestroyImage(device, image, nullptr);
    return nullptr;
  }

  TransitionToColorAttachment(image);

  return std::make_unique<ExternalVkImageBacking>(
      mailbox, format, size, color_space, usage, context_state_, image, memory,
      requirements.size, vk_format);
}

std::unique_ptr<SharedImageBacking> ExternalVkImageFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    base::span<const uint8_t> pixel_data) {
  switch (format) {
    case viz::ETC1:
    case viz::RED_8:
    case viz::LUMINANCE_F16:
    case viz::R16_EXT:
    case viz::BGR_565:
    case viz::RG_88:
    case viz::BGRX_8888:
    case viz::RGBX_1010102:
    case viz::BGRX_1010102:
    case viz::YVU_420:
    case viz::YUV_420_BIPLANAR:
    case viz::UYVY_422:
      // TODO(https://crbug.com/945513): support all formats.
      LOG(ERROR) << "format " << format << " is not supported.";
      return nullptr;
    default:
      break;
  }
  auto sk_color_type = viz::ResourceFormatToClosestSkColorType(
      true /* gpu_compositing */, format);
  auto ii = SkImageInfo::Make(size.width(), size.height(), sk_color_type,
                              kOpaque_SkAlphaType);
  // rows in pixel data are aligned with 4.
  size_t row_bytes = (ii.minRowBytes() + 3) & ~3;
  if (pixel_data.size() != ii.computeByteSize(row_bytes)) {
    LOG(ERROR) << "Initial data does not have expected size.";
    return nullptr;
  }

  auto backing = CreateSharedImage(mailbox, format, size, color_space, usage,
                                   false /* is_thread_safe */);

  if (!backing)
    return nullptr;

  ExternalVkImageBacking* vk_backing =
      static_cast<ExternalVkImageBacking*>(backing.get());

  std::vector<SemaphoreHandle> handles;
  if (!vk_backing->BeginAccess(false /* readonly */, &handles)) {
    LOG(ERROR) << "Failed to request write access of backing.";
    return nullptr;
  }

  DCHECK(handles.empty());

  // Create backend render target from the VkImage.
  GrVkAlloc alloc(vk_backing->memory(), 0 /* offset */,
                  vk_backing->memory_size(), 0 /* flags */);
  GrVkImageInfo vk_image_info(vk_backing->image(), alloc,
                              VK_IMAGE_TILING_OPTIMAL,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                              vk_backing->vk_format(), 1 /* levelCount */);
  GrBackendRenderTarget render_target(size.width(), size.height(),
                                      1 /* sampleCnt */, vk_image_info);
  SkSurfaceProps surface_props(0, SkSurfaceProps::kLegacyFontHost_InitType);
  auto surface = SkSurface::MakeFromBackendRenderTarget(
      context_state_->gr_context(), render_target, kTopLeft_GrSurfaceOrigin,
      sk_color_type, nullptr, &surface_props);
  SkPixmap pixmap(ii, pixel_data.data(), row_bytes);
  surface->writePixels(pixmap, 0, 0);

  auto* vk_implementation =
      context_state_->vk_context_provider()->GetVulkanImplementation();

  VkSemaphore semaphore =
      vk_implementation->CreateExternalSemaphore(vk_backing->device());
  VkDevice device = context_state_->vk_context_provider()
                        ->GetDeviceQueue()
                        ->GetVulkanDevice();
  SemaphoreHandle semaphore_handle =
      vk_implementation->GetSemaphoreHandle(device, semaphore);
  if (!semaphore_handle.is_valid()) {
    LOG(ERROR) << "GetSemaphoreHandle() failed.";
    vkDestroySemaphore(device, semaphore, nullptr /* pAllocator */);
    return nullptr;
  }

  GrBackendSemaphore gr_semaphore;
  gr_semaphore.initVulkan(semaphore);
  if (surface->flushAndSignalSemaphores(1, &gr_semaphore) !=
      GrSemaphoresSubmitted::kYes) {
    LOG(ERROR) << "Failed to flush the surface.";
    vkDestroySemaphore(device, semaphore, nullptr /* pAllocator */);
    return nullptr;
  }
  vk_backing->EndAccess(false /* readonly */, std::move(semaphore_handle));

  context_state_->vk_context_provider()
      ->GetDeviceQueue()
      ->GetFenceHelper()
      ->EnqueueSemaphoreCleanupForSubmittedWork(semaphore);

  return backing;
}

std::unique_ptr<SharedImageBacking> ExternalVkImageFactory::CreateSharedImage(
    const Mailbox& mailbox,
    int client_id,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage) {
  DCHECK(CanImportGpuMemoryBuffer(handle.type));

  if (!gpu::IsImageSizeValidForGpuMemoryBufferFormat(size, buffer_format)) {
    LOG(ERROR) << "Invalid image size for format.";
    return nullptr;
  }

  auto* vk_context_provider = context_state_->vk_context_provider();
  VkDevice vk_device = vk_context_provider->GetDeviceQueue()->GetVulkanDevice();
  VkImage vk_image = VK_NULL_HANDLE;
  VkImageCreateInfo vk_image_info{};
  VkDeviceMemory vk_device_memory = VK_NULL_HANDLE;
  VkDeviceSize memory_size = 0;
  if (!vk_context_provider->GetVulkanImplementation()
           ->CreateImageFromGpuMemoryHandle(vk_device, std::move(handle), size,
                                            &vk_image, &vk_image_info,
                                            &vk_device_memory, &memory_size)) {
    return nullptr;
  }

  VkFormat expected_format = ToVkFormat(viz::GetResourceFormat(buffer_format));
  if (expected_format != vk_image_info.format) {
    DLOG(ERROR) << "BufferFormat doesn't match the buffer";
    vkFreeMemory(vk_device, vk_device_memory, nullptr);
    vkDestroyImage(vk_device, vk_image, nullptr);
    return nullptr;
  }

  TransitionToColorAttachment(vk_image);

  return std::make_unique<ExternalVkImageBacking>(
      mailbox, viz::GetResourceFormat(buffer_format), size, color_space, usage,
      context_state_, vk_image, vk_device_memory, memory_size,
      vk_image_info.format);
}

bool ExternalVkImageFactory::CanImportGpuMemoryBuffer(
    gfx::GpuMemoryBufferType memory_buffer_type) {
  return context_state_->vk_context_provider()
      ->GetVulkanImplementation()
      ->CanImportGpuMemoryBuffer(memory_buffer_type);
}

VkResult ExternalVkImageFactory::CreateExternalVkImage(VkFormat format,
                                                       const gfx::Size& size,
                                                       VkImage* image) {
  VkDevice device = context_state_->vk_context_provider()
                        ->GetDeviceQueue()
                        ->GetVulkanDevice();

  VkExternalMemoryImageCreateInfoKHR external_info;
  external_info.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR;
  external_info.pNext = nullptr;
  external_info.handleTypes = context_state_->vk_context_provider()
                                  ->GetVulkanImplementation()
                                  ->GetExternalImageHandleType();

  VkImageCreateInfo create_info;
  create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  create_info.pNext = &external_info;
  create_info.flags = 0;
  create_info.imageType = VK_IMAGE_TYPE_2D;
  create_info.format = format;
  create_info.extent = {size.width(), size.height(), 1};
  create_info.mipLevels = 1;
  create_info.arrayLayers = 1;
  create_info.samples = VK_SAMPLE_COUNT_1_BIT;
  create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  create_info.usage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  create_info.queueFamilyIndexCount = context_state_->vk_context_provider()
                                          ->GetDeviceQueue()
                                          ->GetVulkanQueueIndex();
  create_info.pQueueFamilyIndices = nullptr;
  create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  return vkCreateImage(device, &create_info, nullptr, image);
}

void ExternalVkImageFactory::TransitionToColorAttachment(VkImage image) {
  if (!command_pool_) {
    command_pool_ = context_state_->vk_context_provider()
                        ->GetDeviceQueue()
                        ->CreateCommandPool();
  }
  std::unique_ptr<VulkanCommandBuffer> command_buffer =
      command_pool_->CreatePrimaryCommandBuffer();
  CHECK(command_buffer->Initialize());
  {
    ScopedSingleUseCommandBufferRecorder recorder(*command_buffer);
    VkImageMemoryBarrier image_memory_barrier;
    image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier.pNext = nullptr;
    image_memory_barrier.srcAccessMask = 0;
    image_memory_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.image = image;
    image_memory_barrier.subresourceRange.aspectMask =
        VK_IMAGE_ASPECT_COLOR_BIT;
    image_memory_barrier.subresourceRange.baseMipLevel = 0;
    image_memory_barrier.subresourceRange.levelCount = 1;
    image_memory_barrier.subresourceRange.baseArrayLayer = 0;
    image_memory_barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(recorder.handle(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &image_memory_barrier);
  }
  command_buffer->Submit(0, nullptr, 0, nullptr);

  context_state_->vk_context_provider()
      ->GetDeviceQueue()
      ->GetFenceHelper()
      ->EnqueueVulkanObjectCleanupForSubmittedWork(std::move(command_buffer));
}

}  // namespace gpu
