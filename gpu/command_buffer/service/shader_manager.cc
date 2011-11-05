// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shader_manager.h"
#include "base/logging.h"
#include "base/string_util.h"

namespace gpu {
namespace gles2 {

ShaderManager::ShaderInfo::ShaderInfo(GLuint service_id, GLenum shader_type)
      : use_count_(0),
        service_id_(service_id),
        shader_type_(shader_type),
        valid_(false) {
}

ShaderManager::ShaderInfo::~ShaderInfo() {
}

void ShaderManager::ShaderInfo::IncUseCount() {
  ++use_count_;
}

void ShaderManager::ShaderInfo::DecUseCount() {
  --use_count_;
  DCHECK_GE(use_count_, 0);
}

void ShaderManager::ShaderInfo::MarkAsDeleted() {
  DCHECK_NE(service_id_, 0u);
  service_id_ = 0;
}

void ShaderManager::ShaderInfo::SetStatus(
    bool valid, const char* log, ShaderTranslatorInterface* translator) {
  valid_ = valid;
  log_info_.reset(log ? new std::string(log) : NULL);
  if (translator && valid) {
    attrib_map_ = translator->attrib_map();
    uniform_map_ = translator->uniform_map();
  } else {
    attrib_map_.clear();
    uniform_map_.clear();
  }
}

const ShaderManager::ShaderInfo::VariableInfo*
    ShaderManager::ShaderInfo::GetAttribInfo(
        const std::string& name) const {
  VariableMap::const_iterator it = attrib_map_.find(name);
  return it != attrib_map_.end() ? &it->second : NULL;
}

const ShaderManager::ShaderInfo::VariableInfo*
    ShaderManager::ShaderInfo::GetUniformInfo(
        const std::string& name) const {
  VariableMap::const_iterator it = uniform_map_.find(name);
  return it != uniform_map_.end() ? &it->second : NULL;
}

ShaderManager::ShaderManager() {}

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

ShaderManager::ShaderInfo* ShaderManager::CreateShaderInfo(
    GLuint client_id,
    GLuint service_id,
    GLenum shader_type) {
  std::pair<ShaderInfoMap::iterator, bool> result =
      shader_infos_.insert(std::make_pair(
          client_id, ShaderInfo::Ref(new ShaderInfo(service_id, shader_type))));
  DCHECK(result.second);
  return result.first->second;
}

ShaderManager::ShaderInfo* ShaderManager::GetShaderInfo(GLuint client_id) {
  ShaderInfoMap::iterator it = shader_infos_.find(client_id);
  return it != shader_infos_.end() ? it->second : NULL;
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

bool ShaderManager::IsOwned(ShaderManager::ShaderInfo* info) {
  for (ShaderInfoMap::iterator it = shader_infos_.begin();
       it != shader_infos_.end(); ++it) {
    if (it->second.get() == info) {
      return true;
    }
  }
  return false;
}

void ShaderManager::RemoveShaderInfoIfUnused(ShaderManager::ShaderInfo* info) {
  DCHECK(info);
  DCHECK(IsOwned(info));
  if (info->IsDeleted() && !info->InUse()) {
    for (ShaderInfoMap::iterator it = shader_infos_.begin();
         it != shader_infos_.end(); ++it) {
      if (it->second.get() == info) {
        shader_infos_.erase(it);
        return;
      }
    }
    NOTREACHED();
  }
}

void ShaderManager::MarkAsDeleted(ShaderManager::ShaderInfo* info) {
  DCHECK(info);
  DCHECK(IsOwned(info));
  info->MarkAsDeleted();
  RemoveShaderInfoIfUnused(info);
}

void ShaderManager::UseShader(ShaderManager::ShaderInfo* info) {
  DCHECK(info);
  DCHECK(IsOwned(info));
  info->IncUseCount();
}

void ShaderManager::UnuseShader(ShaderManager::ShaderInfo* info) {
  DCHECK(info);
  DCHECK(IsOwned(info));
  info->DecUseCount();
  RemoveShaderInfoIfUnused(info);
}

}  // namespace gles2
}  // namespace gpu


