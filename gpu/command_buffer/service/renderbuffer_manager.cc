// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/renderbuffer_manager.h"
#include "base/logging.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

namespace gpu {
namespace gles2 {

void RenderbufferManager::CreateRenderbufferInfo(GLuint renderbuffer_id) {
  std::pair<RenderbufferInfoMap::iterator, bool> result =
      renderbuffer_infos_.insert(
          std::make_pair(
              renderbuffer_id,
              RenderbufferInfo::Ref(new RenderbufferInfo(renderbuffer_id))));
  DCHECK(result.second);
}

RenderbufferManager::RenderbufferInfo* RenderbufferManager::GetRenderbufferInfo(
    GLuint renderbuffer_id) {
  RenderbufferInfoMap::iterator it = renderbuffer_infos_.find(renderbuffer_id);
  return it != renderbuffer_infos_.end() ? it->second : NULL;
}

void RenderbufferManager::RemoveRenderbufferInfo(GLuint renderbuffer_id) {
  RenderbufferInfoMap::iterator it = renderbuffer_infos_.find(renderbuffer_id);
  if (it != renderbuffer_infos_.end()) {
    it->second->MarkAsDeleted();
    renderbuffer_infos_.erase(renderbuffer_id);
  }
}

}  // namespace gles2
}  // namespace gpu


