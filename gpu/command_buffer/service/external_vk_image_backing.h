// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_BACKING_H_

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/vulkan/semaphore_handle.h"
#include "gpu/vulkan/vulkan_device_queue.h"

namespace gpu {

class ExternalVkImageBacking : public SharedImageBacking {
 public:
  ExternalVkImageBacking(const Mailbox& mailbox,
                         viz::ResourceFormat format,
                         const gfx::Size& size,
                         const gfx::ColorSpace& color_space,
                         uint32_t usage,
                         SharedContextState* context_state,
                         VkImage image,
                         VkDeviceMemory memory,
                         size_t memory_size,
                         VkFormat vk_format);
  ~ExternalVkImageBacking() override;

  VkImage image() { return image_; }
  VkDeviceMemory memory() { return memory_; }
  size_t memory_size() { return memory_size_; }
  VkFormat vk_format() { return vk_format_; }
  SharedContextState* context_state() { return context_state_; }
  VkDevice device() {
    return context_state_->vk_context_provider()
        ->GetDeviceQueue()
        ->GetVulkanDevice();
  }

  // Notifies the backing that an access will start. Return false if there is
  // currently any other conflict access in progress. Otherwise, returns true
  // and semaphore handles which will be waited on before accessing.
  bool BeginAccess(bool readonly,
                   std::vector<SemaphoreHandle>* semaphore_handles);

  // Notifies the backing that an access has ended. The representation must
  // provide a semaphore handle that has been signaled at the end of the write
  // access.
  void EndAccess(bool readonly, SemaphoreHandle semaphore_handle);

  // SharedImageBacking implementation.
  bool IsCleared() const override;
  void SetCleared() override;
  void Update() override;
  void Destroy() override;
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override;

 protected:
  // SharedImageBacking implementation.
  std::unique_ptr<SharedImageRepresentationGLTexture> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;
  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override;
  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

 private:
  SharedContextState* const context_state_;
  VkImage image_;
  VkDeviceMemory memory_;
  SemaphoreHandle write_semaphore_handle_;
  std::vector<SemaphoreHandle> read_semaphore_handles_;
  size_t memory_size_;
  bool is_cleared_ = false;
  VkFormat vk_format_;
  bool is_write_in_progress_ = false;
  uint32_t reads_in_progress_ = 0;
  gles2::Texture* texture_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ExternalVkImageBacking);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_BACKING_H_
