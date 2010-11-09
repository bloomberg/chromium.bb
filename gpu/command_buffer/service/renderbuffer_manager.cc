// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/renderbuffer_manager.h"
#include "base/logging.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

namespace gpu {
namespace gles2 {

RenderbufferManager::RenderbufferManager(GLint max_renderbuffer_size)
    : max_renderbuffer_size_(max_renderbuffer_size) {
}

RenderbufferManager::~RenderbufferManager() {
  DCHECK(renderbuffer_infos_.empty());
}

void RenderbufferManager::Destroy(bool have_context) {
  while (!renderbuffer_infos_.empty()) {
    if (have_context) {
      RenderbufferInfo* info = renderbuffer_infos_.begin()->second;
      if (!info->IsDeleted()) {
        GLuint service_id = info->service_id();
        glDeleteRenderbuffersEXT(1, &service_id);
        info->MarkAsDeleted();
      }
    }
    renderbuffer_infos_.erase(renderbuffer_infos_.begin());
  }
}

void RenderbufferManager::CreateRenderbufferInfo(
    GLuint client_id, GLuint service_id) {
  std::pair<RenderbufferInfoMap::iterator, bool> result =
      renderbuffer_infos_.insert(
          std::make_pair(
              client_id,
              RenderbufferInfo::Ref(new RenderbufferInfo(service_id))));
  DCHECK(result.second);
}

RenderbufferManager::RenderbufferInfo* RenderbufferManager::GetRenderbufferInfo(
    GLuint client_id) {
  RenderbufferInfoMap::iterator it = renderbuffer_infos_.find(client_id);
  return it != renderbuffer_infos_.end() ? it->second : NULL;
}

void RenderbufferManager::RemoveRenderbufferInfo(GLuint client_id) {
  RenderbufferInfoMap::iterator it = renderbuffer_infos_.find(client_id);
  if (it != renderbuffer_infos_.end()) {
    it->second->MarkAsDeleted();
    renderbuffer_infos_.erase(it);
  }
}

bool RenderbufferManager::GetClientId(
    GLuint service_id, GLuint* client_id) const {
  // This doesn't need to be fast. It's only used during slow queries.
  for (RenderbufferInfoMap::const_iterator it = renderbuffer_infos_.begin();
       it != renderbuffer_infos_.end(); ++it) {
    if (it->second->service_id() == service_id) {
      *client_id = it->first;
      return true;
    }
  }
  return false;
}

}  // namespace gles2
}  // namespace gpu


