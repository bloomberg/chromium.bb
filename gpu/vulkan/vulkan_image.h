// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VULKAN_IMAGE_H_
#define GPU_VULKAN_VULKAN_IMAGE_H_

#include <vulkan/vulkan.h>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/optional.h"
#include "base/util/type_safety/pass_key.h"
#include "build/build_config.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif

#if defined(OS_FUCHSIA)
#include <lib/zx/vmo.h>
#endif

namespace gpu {

class VulkanDeviceQueue;

class COMPONENT_EXPORT(VULKAN) VulkanImage {
 public:
  explicit VulkanImage(util::PassKey<VulkanImage> pass_key);
  ~VulkanImage();

  VulkanImage(VulkanImage&) = delete;
  VulkanImage& operator=(VulkanImage&) = delete;

  static std::unique_ptr<VulkanImage> Create(
      VulkanDeviceQueue* device_queue,
      const gfx::Size& size,
      VkFormat format,
      VkImageUsageFlags usage,
      VkImageCreateFlags flags = 0,
      VkImageTiling image_tiling = VK_IMAGE_TILING_OPTIMAL,
      void* vk_image_create_info_next = nullptr,
      void* vk_memory_allocation_info_next = nullptr);

  // Create VulkanImage with external memory, it can be exported and used by
  // foreign API
  static std::unique_ptr<VulkanImage> CreateWithExternalMemory(
      VulkanDeviceQueue* device_queue,
      const gfx::Size& size,
      VkFormat format,
      VkImageUsageFlags usage,
      VkImageCreateFlags flags = 0,
      VkImageTiling image_tiling = VK_IMAGE_TILING_OPTIMAL);

  static std::unique_ptr<VulkanImage> CreateFromGpuMemoryBufferHandle(
      VulkanDeviceQueue* device_queue,
      gfx::GpuMemoryBufferHandle gmb_handle,
      const gfx::Size& size,
      VkFormat format,
      VkImageUsageFlags usage,
      VkImageCreateFlags flags = 0,
      VkImageTiling image_tiling = VK_IMAGE_TILING_OPTIMAL);

  static std::unique_ptr<VulkanImage> Create(
      VulkanDeviceQueue* device_queue,
      VkImage image,
      VkDeviceMemory device_memory,
      const gfx::Size& size,
      VkFormat format,
      VkImageTiling image_tiling,
      VkDeviceSize device_size,
      uint32_t memory_type_index,
      base::Optional<VulkanYCbCrInfo>& ycbcr_info);

  void Destroy();

#if defined(OS_POSIX)
  base::ScopedFD GetMemoryFd(VkExternalMemoryHandleTypeFlagBits handle_type =
                                 VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT);
#endif

#if defined(OS_WIN)
  base::win::ScopedHandle GetMemoryHandle(
      VkExternalMemoryHandleTypeFlagBits handle_type =
          VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT);
#endif

#if defined(OS_FUCHSIA)
  zx::vmo GetMemoryZirconHandle();
#endif

  VulkanDeviceQueue* device_queue() const { return device_queue_; }
  const gfx::Size& size() const { return size_; }
  VkFormat format() const { return format_; }
  VkImageCreateFlags flags() const { return flags_; }
  VkDeviceSize device_size() const { return device_size_; }
  uint32_t memory_type_index() const { return memory_type_index_; }
  VkImageTiling image_tiling() const { return image_tiling_; }
  VkImageLayout image_layout() const { return image_layout_; }
  void set_image_layout(VkImageLayout layout) { image_layout_ = layout; }
  uint32_t queue_family_index() const { return queue_family_index_; }
  void set_queue_family_index(uint32_t index) { queue_family_index_ = index; }
  const base::Optional<VulkanYCbCrInfo>& ycbcr_info() const {
    return ycbcr_info_;
  }
  VkImage image() const { return image_; }
  VkDeviceMemory device_memory() const { return device_memory_; }
  VkExternalMemoryHandleTypeFlags handle_types() const { return handle_types_; }

 private:
  bool Initialize(VulkanDeviceQueue* device_queue,
                  const gfx::Size& size,
                  VkFormat format,
                  VkImageUsageFlags usage,
                  VkImageCreateFlags flags,
                  VkImageTiling image_tiling,
                  void* image_create_info_next,
                  void* memory_allocation_info_next,
                  const VkMemoryRequirements* requirements);
  bool InitializeWithExternalMemory(VulkanDeviceQueue* device_queue,
                                    const gfx::Size& size,
                                    VkFormat format,
                                    VkImageUsageFlags usage,
                                    VkImageCreateFlags flags,
                                    VkImageTiling image_tiling);
  bool InitializeFromGpuMemoryBufferHandle(
      VulkanDeviceQueue* device_queue,
      gfx::GpuMemoryBufferHandle gmb_handle,
      const gfx::Size& size,
      VkFormat format,
      VkImageUsageFlags usage,
      VkImageCreateFlags flags,
      VkImageTiling image_tiling);

  VulkanDeviceQueue* device_queue_ = nullptr;
  gfx::Size size_;
  VkFormat format_ = VK_FORMAT_UNDEFINED;
  VkImageCreateFlags flags_ = 0;
  VkDeviceSize device_size_ = 0;
  uint32_t memory_type_index_ = 0;
  VkImageTiling image_tiling_ = VK_IMAGE_TILING_OPTIMAL;
  VkImageLayout image_layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
  uint32_t queue_family_index_ = VK_QUEUE_FAMILY_IGNORED;
  base::Optional<VulkanYCbCrInfo> ycbcr_info_;
  VkImage image_ = VK_NULL_HANDLE;
  VkDeviceMemory device_memory_ = VK_NULL_HANDLE;
  VkExternalMemoryHandleTypeFlags handle_types_ = 0;
};

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_IMAGE_H_
