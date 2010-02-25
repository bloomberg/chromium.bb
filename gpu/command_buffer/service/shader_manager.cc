// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shader_manager.h"
#include "base/logging.h"

namespace gpu {
namespace gles2 {

void ShaderManager::CreateShaderInfo(GLuint shader_id) {
  std::pair<ShaderInfoMap::iterator, bool> result =
      shader_infos_.insert(std::make_pair(
          shader_id, ShaderInfo::Ref(new ShaderInfo(shader_id))));
  DCHECK(result.second);
}

ShaderManager::ShaderInfo* ShaderManager::GetShaderInfo(GLuint shader_id) {
  ShaderInfoMap::iterator it = shader_infos_.find(shader_id);
  return it != shader_infos_.end() ? it->second : NULL;
}

void ShaderManager::RemoveShaderInfo(GLuint shader_id) {
  ShaderInfoMap::iterator it = shader_infos_.find(shader_id);
  if (it != shader_infos_.end()) {
    it->second->MarkAsDeleted();
    shader_infos_.erase(it);
  }
}

}  // namespace gles2
}  // namespace gpu


