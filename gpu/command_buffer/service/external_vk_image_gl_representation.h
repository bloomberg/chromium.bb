// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_GL_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_GL_REPRESENTATION_H_

#include <memory>

#include "gpu/command_buffer/service/external_vk_image_backing.h"
#include "gpu/command_buffer/service/shared_image_representation.h"

namespace gpu {

// ExternalVkImageGLRepresentationShared implements BeginAccess and EndAccess
// methods for ExternalVkImageGLRepresentation and
// ExternalVkImageGLPassthroughRepresentation.
class ExternalVkImageGLRepresentationShared {
 public:
  ExternalVkImageGLRepresentationShared(SharedImageBacking* backing,
                                        GLuint texture_service_id);
  ~ExternalVkImageGLRepresentationShared() = default;

  bool BeginAccess(GLenum mode);
  void EndAccess();

  ExternalVkImageBacking* backing_impl() { return backing_; }

 private:
  gpu::VulkanImplementation* vk_implementation() {
    return backing_impl()
        ->context_state()
        ->vk_context_provider()
        ->GetVulkanImplementation();
  }

  VkDevice vk_device() {
    return backing_impl()
        ->context_state()
        ->vk_context_provider()
        ->GetDeviceQueue()
        ->GetVulkanDevice();
  }

  VkQueue vk_queue() {
    return backing_impl()
        ->context_state()
        ->vk_context_provider()
        ->GetDeviceQueue()
        ->GetVulkanQueue();
  }

  gl::GLApi* api() { return gl::g_current_gl_context; }

  GLuint ImportVkSemaphoreIntoGL(SemaphoreHandle handle);
  void DestroyEndAccessSemaphore();

  ExternalVkImageBacking* backing_;
  GLuint texture_service_id_ = 0;
  GLenum current_access_mode_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ExternalVkImageGLRepresentationShared);
};

class ExternalVkImageGLRepresentation
    : public SharedImageRepresentationGLTexture {
 public:
  ExternalVkImageGLRepresentation(SharedImageManager* manager,
                                  SharedImageBacking* backing,
                                  MemoryTypeTracker* tracker,
                                  gles2::Texture* texture,
                                  GLuint texture_service_id);
  ~ExternalVkImageGLRepresentation() override;

  // SharedImageRepresentationGLTexture implementation.
  gles2::Texture* GetTexture() override;
  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

 private:
  gles2::Texture* texture_ = nullptr;
  ExternalVkImageGLRepresentationShared representation_shared_;

  DISALLOW_COPY_AND_ASSIGN(ExternalVkImageGLRepresentation);
};

class ExternalVkImageGLPassthroughRepresentation
    : public SharedImageRepresentationGLTexturePassthrough {
 public:
  ExternalVkImageGLPassthroughRepresentation(SharedImageManager* manager,
                                             SharedImageBacking* backing,
                                             MemoryTypeTracker* tracker,
                                             GLuint texture_service_id);
  ~ExternalVkImageGLPassthroughRepresentation() override;

  // SharedImageRepresentationGLTexturePassthrough implementation.
  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough()
      override;
  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

 private:
  ExternalVkImageGLRepresentationShared representation_shared_;

  DISALLOW_COPY_AND_ASSIGN(ExternalVkImageGLPassthroughRepresentation);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_GL_REPRESENTATION_H_
