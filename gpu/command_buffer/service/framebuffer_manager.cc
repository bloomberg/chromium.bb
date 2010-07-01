// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "base/logging.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"

namespace gpu {
namespace gles2 {

FramebufferManager::~FramebufferManager() {
  DCHECK(framebuffer_infos_.empty());
}

void FramebufferManager::Destroy(bool have_context) {
  while (!framebuffer_infos_.empty()) {
    if (have_context) {
      FramebufferInfo* info = framebuffer_infos_.begin()->second;
      if (!info->IsDeleted()) {
        GLuint service_id = info->service_id();
        glDeleteFramebuffersEXT(1, &service_id);
        info->MarkAsDeleted();
      }
    }
    framebuffer_infos_.erase(framebuffer_infos_.begin());
  }
}

void FramebufferManager::CreateFramebufferInfo(
    GLuint client_id, GLuint service_id) {
  std::pair<FramebufferInfoMap::iterator, bool> result =
      framebuffer_infos_.insert(
          std::make_pair(
              client_id,
              FramebufferInfo::Ref(new FramebufferInfo(service_id))));
  DCHECK(result.second);
}

FramebufferManager::FramebufferInfo* FramebufferManager::GetFramebufferInfo(
    GLuint client_id) {
  FramebufferInfoMap::iterator it = framebuffer_infos_.find(client_id);
  return it != framebuffer_infos_.end() ? it->second : NULL;
}

void FramebufferManager::RemoveFramebufferInfo(GLuint client_id) {
  FramebufferInfoMap::iterator it = framebuffer_infos_.find(client_id);
  if (it != framebuffer_infos_.end()) {
    it->second->MarkAsDeleted();
    framebuffer_infos_.erase(it);
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

bool FramebufferManager::GetClientId(
    GLuint service_id, GLuint* client_id) const {
  // This doesn't need to be fast. It's only used during slow queries.
  for (FramebufferInfoMap::const_iterator it = framebuffer_infos_.begin();
       it != framebuffer_infos_.end(); ++it) {
    if (it->second->service_id() == service_id) {
      *client_id = it->first;
      return true;
    }
  }
  return false;
}

}  // namespace gles2
}  // namespace gpu


