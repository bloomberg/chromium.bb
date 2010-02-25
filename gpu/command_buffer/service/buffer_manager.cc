// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/buffer_manager.h"
#include "base/logging.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

namespace gpu {
namespace gles2 {

void BufferManager::CreateBufferInfo(GLuint buffer_id) {
  std::pair<BufferInfoMap::iterator, bool> result =
      buffer_infos_.insert(
          std::make_pair(buffer_id,
                         BufferInfo::Ref(new BufferInfo(buffer_id))));
  DCHECK(result.second);
}

BufferManager::BufferInfo* BufferManager::GetBufferInfo(
    GLuint buffer_id) {
  BufferInfoMap::iterator it = buffer_infos_.find(buffer_id);
  return it != buffer_infos_.end() ? it->second : NULL;
}

void BufferManager::RemoveBufferInfo(GLuint buffer_id) {
  BufferInfoMap::iterator it = buffer_infos_.find(buffer_id);
  if (it != buffer_infos_.end()) {
    it->second->MarkAsDeleted();
    buffer_infos_.erase(buffer_id);
  }
}

GLuint BufferManager::BufferInfo::GetMaxValueForRange(
    GLuint offset, GLsizei count, GLenum type) {
  // TODO(gman): Scan the values in the given range and cache their results.
  return 0u;
}

}  // namespace gles2
}  // namespace gpu


