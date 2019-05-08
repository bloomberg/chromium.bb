// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/external_vk_image_backing.h"

#include <utility>

#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/external_vk_image_gl_representation.h"
#include "gpu/command_buffer/service/external_vk_image_skia_representation.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "ui/gl/gl_context.h"

#if defined(OS_FUCHSIA)
#include "gpu/vulkan/fuchsia/vulkan_fuchsia_ext.h"
#endif

#if defined(OS_LINUX)
#define GL_HANDLE_TYPE_OPAQUE_FD_EXT 0x9586
#endif

namespace gpu {

ExternalVkImageBacking::ExternalVkImageBacking(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    SharedContextState* context_state,
    VkImage image,
    VkDeviceMemory memory,
    size_t memory_size,
    VkFormat vk_format)
    : SharedImageBacking(mailbox,
                         format,
                         size,
                         color_space,
                         usage,
                         memory_size,
                         false /* is_thread_safe */),
      context_state_(context_state),
      image_(image),
      memory_(memory),
      memory_size_(memory_size),
      vk_format_(vk_format) {}

ExternalVkImageBacking::~ExternalVkImageBacking() {
  // Destroy() will do any necessary cleanup.
}

bool ExternalVkImageBacking::BeginAccess(
    bool readonly,
    std::vector<SemaphoreHandle>* semaphore_handles) {
  DCHECK(semaphore_handles);
  DCHECK(semaphore_handles->empty());
  if (is_write_in_progress_) {
    LOG(ERROR) << "Unable to begin read or write access because another write "
                  "access is in progress";
    return false;
  }

  if (reads_in_progress_ && !readonly) {
    LOG(ERROR)
        << "Unable to begin write access because a read access is in progress";
    return false;
  }

  if (readonly) {
    DLOG_IF(ERROR, reads_in_progress_)
        << "Concurrent reading may cause problem.";

    ++reads_in_progress_;
    // A semaphore will become unsignaled, when it has been signaled and waited,
    // so it is not safe to reuse it.
    if (write_semaphore_handle_.is_valid())
      semaphore_handles->push_back(std::move(write_semaphore_handle_));
  } else {
    is_write_in_progress_ = true;
    *semaphore_handles = std::move(read_semaphore_handles_);
    read_semaphore_handles_.clear();
    if (write_semaphore_handle_.is_valid())
      semaphore_handles->push_back(std::move(write_semaphore_handle_));
  }
  return true;
}

void ExternalVkImageBacking::EndAccess(bool readonly,
                                       SemaphoreHandle semaphore_handle) {
  DCHECK(semaphore_handle.is_valid());

  if (readonly) {
    DCHECK_GT(reads_in_progress_, 0u);
    --reads_in_progress_;
    read_semaphore_handles_.push_back(std::move(semaphore_handle));
  } else {
    DCHECK(is_write_in_progress_);
    DCHECK(!write_semaphore_handle_.is_valid());
    DCHECK(read_semaphore_handles_.empty());
    is_write_in_progress_ = false;
    write_semaphore_handle_ = std::move(semaphore_handle);
  }
}

bool ExternalVkImageBacking::IsCleared() const {
  return is_cleared_;
}

void ExternalVkImageBacking::SetCleared() {
  is_cleared_ = true;
}

void ExternalVkImageBacking::Update() {}

void ExternalVkImageBacking::Destroy() {
  auto* fence_helper = context_state()
                           ->vk_context_provider()
                           ->GetDeviceQueue()
                           ->GetFenceHelper();
  fence_helper->EnqueueImageCleanupForSubmittedWork(image_, memory_);
  image_ = VK_NULL_HANDLE;
  memory_ = VK_NULL_HANDLE;

  if (texture_)
    texture_->RemoveLightweightRef(have_context());
}

bool ExternalVkImageBacking::ProduceLegacyMailbox(
    MailboxManager* mailbox_manager) {
  // It is not safe to produce a legacy mailbox because it would bypass the
  // synchronization between Vulkan and GL that is implemented in the
  // representation classes.
  return false;
}

std::unique_ptr<SharedImageRepresentationGLTexture>
ExternalVkImageBacking::ProduceGLTexture(SharedImageManager* manager,
                                         MemoryTypeTracker* tracker) {
#if defined(OS_FUCHSIA)
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
#elif defined(OS_LINUX)
  if (!texture_) {
    VkMemoryGetFdInfoKHR get_fd_info;
    get_fd_info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
    get_fd_info.pNext = nullptr;
    get_fd_info.memory = memory_;
    get_fd_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;

    int memory_fd = -1;
    vkGetMemoryFdKHR(device(), &get_fd_info, &memory_fd);
    if (memory_fd < 0) {
      LOG(ERROR) << "Unable to extract file descriptor out of external VkImage";
      return nullptr;
    }

    gl::GLApi* api = gl::g_current_gl_context;

    constexpr GLenum target = GL_TEXTURE_2D;
    constexpr GLenum get_target = GL_TEXTURE_BINDING_2D;
    GLuint internal_format = viz::TextureStorageFormat(format());

    GLuint memory_object;
    api->glCreateMemoryObjectsEXTFn(1, &memory_object);
    api->glImportMemoryFdEXTFn(memory_object, memory_size_,
                               GL_HANDLE_TYPE_OPAQUE_FD_EXT, memory_fd);
    GLuint texture_service_id;
    api->glGenTexturesFn(1, &texture_service_id);

    GLint old_texture_binding = 0;
    api->glGetIntegervFn(get_target, &old_texture_binding);
    api->glBindTextureFn(target, texture_service_id);
    api->glTexParameteriFn(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    api->glTexParameteriFn(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    api->glTexParameteriFn(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    api->glTexParameteriFn(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    api->glTexStorageMem2DEXTFn(GL_TEXTURE_2D, 1, internal_format,
                                size().width(), size().height(), memory_object,
                                0);

    texture_ = new gles2::Texture(texture_service_id);
    texture_->SetLightweightRef();
    texture_->SetTarget(target, 1);
    texture_->sampler_state_.min_filter = GL_LINEAR;
    texture_->sampler_state_.mag_filter = GL_LINEAR;
    texture_->sampler_state_.wrap_t = GL_CLAMP_TO_EDGE;
    texture_->sampler_state_.wrap_s = GL_CLAMP_TO_EDGE;
    // If the backing is already cleared, no need to clear it again.
    gfx::Rect cleared_rect;
    if (is_cleared_)
      cleared_rect = gfx::Rect(size());

    GLenum gl_format = viz::GLDataFormat(format());
    GLenum gl_type = viz::GLDataType(format());
    texture_->SetLevelInfo(target, 0, internal_format, size().width(),
                           size().height(), 1, 0, gl_format, gl_type,
                           cleared_rect);
    texture_->SetImmutable(true);

    api->glBindTextureFn(target, old_texture_binding);
  }
  return std::make_unique<ExternalVkImageGlRepresentation>(
      manager, this, tracker, texture_, texture_->service_id());
#else  // !defined(OS_LINUX) && !defined(OS_FUCHSIA)
#error Unsupported OS
#endif
}

std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
ExternalVkImageBacking::ProduceGLTexturePassthrough(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  // Passthrough command decoder is not currently used on Linux.
  return nullptr;
}

std::unique_ptr<SharedImageRepresentationSkia>
ExternalVkImageBacking::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  // This backing type is only used when vulkan is enabled, so SkiaRenderer
  // should also be using Vulkan.
  DCHECK_EQ(context_state_, context_state.get());
  DCHECK(context_state->GrContextIsVulkan());
  return std::make_unique<ExternalVkImageSkiaRepresentation>(manager, this,
                                                             tracker);
}

}  // namespace gpu
