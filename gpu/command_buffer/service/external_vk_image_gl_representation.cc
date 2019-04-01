// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/external_vk_image_gl_representation.h"

#include <utility>
#include <vector>

#include "base/posix/eintr_wrapper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_implementation.h"

#define GL_LAYOUT_COLOR_ATTACHMENT_EXT 0x958E
#define GL_HANDLE_TYPE_OPAQUE_FD_EXT 0x9586

namespace gpu {

ExternalVkImageGlRepresentation::ExternalVkImageGlRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    gles2::Texture* texture,
    GLuint texture_service_id)
    : SharedImageRepresentationGLTexture(manager, backing, tracker),
      texture_(texture),
      texture_service_id_(texture_service_id) {}

ExternalVkImageGlRepresentation::~ExternalVkImageGlRepresentation() {
  texture_->RemoveLightweightRef(backing_impl()->have_context());
}

gles2::Texture* ExternalVkImageGlRepresentation::GetTexture() {
  return texture_;
}

bool ExternalVkImageGlRepresentation::BeginAccess(GLenum mode) {
  // There should not be multiple accesses in progress on the same
  // representation.
  if (current_access_mode_) {
    LOG(ERROR) << "BeginAccess called on ExternalVkImageGlRepresentation before"
               << " the previous access ended.";
    return false;
  }

  DCHECK(mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM ||
         mode == GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
  const bool readonly = (mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);

  std::vector<base::ScopedFD> fds;

  if (!backing_impl()->BeginAccess(readonly, &fds))
    return false;

  for (auto& fd : fds) {
    GLuint gl_semaphore = ImportVkSemaphoreIntoGL(std::move(fd));
    if (gl_semaphore) {
      GLenum src_layout = GL_LAYOUT_COLOR_ATTACHMENT_EXT;
      api()->glWaitSemaphoreEXTFn(gl_semaphore, 0, nullptr, 1,
                                  &texture_service_id_, &src_layout);
      api()->glDeleteSemaphoresEXTFn(1, &gl_semaphore);
    }
  }
  current_access_mode_ = mode;
  return true;
}

void ExternalVkImageGlRepresentation::EndAccess() {
  if (!current_access_mode_) {
    // TODO(crbug.com/933452): We should be able to handle this failure more
    // gracefully rather than shutting down the whole process.
    LOG(ERROR) << "EndAccess called on ExternalVkImageGlRepresentation before "
               << "BeginAccess";
    return;
  }

  DCHECK(current_access_mode_ == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM ||
         current_access_mode_ ==
             GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
  const bool readonly =
      (current_access_mode_ == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  current_access_mode_ = 0;

  VkSemaphore semaphore = backing_impl()->CreateExternalVkSemaphore();
  if (semaphore == VK_NULL_HANDLE) {
    // TODO(crbug.com/933452): We should be able to handle this failure more
    // gracefully rather than shutting down the whole process.
    LOG(FATAL) << "Unable to create a VkSemaphore in "
               << "ExternalVkImageGlRepresentation for synchronization with "
               << "Vulkan";
    return;
  }

  base::ScopedFD fd;
  bool result =
      vk_implementation()->GetSemaphoreFdKHR(vk_device(), semaphore, &fd);
  vkDestroySemaphore(backing_impl()->device(), semaphore, nullptr);
  if (!result) {
    LOG(FATAL) << "Unable to export VkSemaphore into GL in "
               << "ExternalVkImageGlRepresentation for synchronization with "
               << "Vulkan";
    return;
  }

  base::ScopedFD dup_fd(HANDLE_EINTR(dup(fd.get())));
  GLuint gl_semaphore = ImportVkSemaphoreIntoGL(std::move(dup_fd));

  if (!gl_semaphore) {
    // TODO(crbug.com/933452): We should be able to handle this failure more
    // gracefully rather than shutting down the whole process.
    LOG(FATAL) << "Unable to export VkSemaphore into GL in "
               << "ExternalVkImageGlRepresentation for synchronization with "
               << "Vulkan";
    return;
  }

  GLenum dst_layout = GL_LAYOUT_COLOR_ATTACHMENT_EXT;
  api()->glSignalSemaphoreEXTFn(gl_semaphore, 0, nullptr, 1,
                                &texture_service_id_, &dst_layout);
  api()->glDeleteSemaphoresEXTFn(1, &gl_semaphore);
  backing_impl()->EndAccess(readonly, std::move(fd));
}

GLuint ExternalVkImageGlRepresentation::ImportVkSemaphoreIntoGL(
    base::ScopedFD fd) {
  if (!fd.is_valid())
    return 0;
  gl::GLApi* api = gl::g_current_gl_context;
  GLuint gl_semaphore;
  api->glGenSemaphoresEXTFn(1, &gl_semaphore);
  api->glImportSemaphoreFdEXTFn(gl_semaphore, GL_HANDLE_TYPE_OPAQUE_FD_EXT,
                                fd.release());

  return gl_semaphore;
}

}  // namespace gpu
