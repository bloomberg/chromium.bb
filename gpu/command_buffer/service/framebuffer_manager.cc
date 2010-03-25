// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "base/logging.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"

namespace gpu {
namespace gles2 {

void FramebufferManager::CreateFramebufferInfo(GLuint framebuffer_id) {
  std::pair<FramebufferInfoMap::iterator, bool> result =
      framebuffer_infos_.insert(
          std::make_pair(
              framebuffer_id,
              FramebufferInfo::Ref(new FramebufferInfo(framebuffer_id))));
  DCHECK(result.second);
}

FramebufferManager::FramebufferInfo* FramebufferManager::GetFramebufferInfo(
    GLuint framebuffer_id) {
  FramebufferInfoMap::iterator it = framebuffer_infos_.find(framebuffer_id);
  return it != framebuffer_infos_.end() ? it->second : NULL;
}

void FramebufferManager::RemoveFramebufferInfo(GLuint framebuffer_id) {
  FramebufferInfoMap::iterator it = framebuffer_infos_.find(framebuffer_id);
  if (it != framebuffer_infos_.end()) {
    it->second->MarkAsDeleted();
    framebuffer_infos_.erase(framebuffer_id);
  }
}

void FramebufferManager::FramebufferInfo::AttachRenderbuffer(
    GLenum attachment, RenderbufferManager::RenderbufferInfo* renderbuffer) {
  DCHECK(attachment == GL_COLOR_ATTACHMENT0 ||
         attachment == GL_DEPTH_ATTACHMENT ||
         attachment == GL_STENCIL_ATTACHMENT);
  if (renderbuffer) {
    renderbuffers_[attachment] =
        RenderbufferManager::RenderbufferInfo::Ref(renderbuffer);
  } else {
    renderbuffers_.erase(attachment);
  }
}

}  // namespace gles2
}  // namespace gpu


