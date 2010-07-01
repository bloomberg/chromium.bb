// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shader_manager.h"
#include "base/logging.h"

namespace gpu {
namespace gles2 {

ShaderManager::~ShaderManager() {
  DCHECK(shader_infos_.empty());
}

void ShaderManager::Destroy(bool have_context) {
  while (!shader_infos_.empty()) {
    if (have_context) {
      ShaderInfo* info = shader_infos_.begin()->second;
      if (!info->IsDeleted()) {
        glDeleteShader(info->service_id());
        info->MarkAsDeleted();
      }
    }
    shader_infos_.erase(shader_infos_.begin());
  }
}

void ShaderManager::CreateShaderInfo(GLuint client_id,
                                     GLuint service_id,
                                     GLenum shader_type) {
  std::pair<ShaderInfoMap::iterator, bool> result =
      shader_infos_.insert(std::make_pair(
          client_id, ShaderInfo::Ref(new ShaderInfo(service_id, shader_type))));
  DCHECK(result.second);
}

ShaderManager::ShaderInfo* ShaderManager::GetShaderInfo(GLuint client_id) {
  ShaderInfoMap::iterator it = shader_infos_.find(client_id);
  return it != shader_infos_.end() ? it->second : NULL;
}

void ShaderManager::RemoveShaderInfo(GLuint client_id) {
  ShaderInfoMap::iterator it = shader_infos_.find(client_id);
  if (it != shader_infos_.end()) {
    it->second->MarkAsDeleted();
    shader_infos_.erase(it);
  }
}

bool ShaderManager::GetClientId(GLuint service_id, GLuint* client_id) const {
  // This doesn't need to be fast. It's only used during slow queries.
  for (ShaderInfoMap::const_iterator it = shader_infos_.begin();
       it != shader_infos_.end(); ++it) {
    if (it->second->service_id() == service_id) {
      *client_id = it->first;
      return true;
    }
  }
  return false;
}

}  // namespace gles2
}  // namespace gpu


