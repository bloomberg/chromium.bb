// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/buffer_manager.h"
#include "base/logging.h"

namespace gpu {
namespace gles2 {

void BufferManager::CreateBufferInfo(GLuint buffer) {
  std::pair<BufferInfoMap::iterator, bool> result =
      buffer_infos_.insert(std::make_pair(buffer, BufferInfo()));
  DCHECK(result.second);
}

BufferManager::BufferInfo* BufferManager::GetBufferInfo(
    GLuint buffer) {
  BufferInfoMap::iterator it = buffer_infos_.find(buffer);
  return it != buffer_infos_.end() ? &it->second : NULL;
}

void BufferManager::RemoveBufferInfo(GLuint buffer_id) {
  buffer_infos_.erase(buffer_id);
}

GLuint BufferManager::BufferInfo::GetMaxValueForRange(
    GLuint offset, GLsizei count, GLenum type) {
  // TODO(gman): Scan the values in the given range and cache their results.
  return 0u;
}

}  // namespace gles2
}  // namespace gpu


