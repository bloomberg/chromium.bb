// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shader_manager.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/string_util.h"

namespace gpu {
namespace gles2 {

Shader::Shader(GLuint service_id, GLenum shader_type)
      : use_count_(0),
        service_id_(service_id),
        shader_type_(shader_type),
        valid_(false) {
}

Shader::~Shader() {
}

void Shader::DoCompile(ShaderTranslatorInterface* translator,
                       TranslatedShaderSourceType type) {
  // Translate GL ES 2.0 shader to Desktop GL shader and pass that to
  // glShaderSource and then glCompileShader.
  const char* source_for_driver = source_.c_str();
  if (translator) {
    valid_ = translator->Translate(source_,
                                   &log_info_,
                                   &translated_source_,
                                   &attrib_map_,
                                   &uniform_map_,
                                   &varying_map_,
                                   &name_map_);
    if (!valid_) {
      return;
    }
    signature_source_ = source_;
    source_for_driver = translated_source_.c_str();
  }

  glShaderSource(service_id_, 1, &source_for_driver, NULL);
  glCompileShader(service_id_);
  if (type == kANGLE) {
    GLint max_len = 0;
    glGetShaderiv(service_id_,
                  GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE,
                  &max_len);
    scoped_ptr<char[]> buffer(new char[max_len]);
    GLint len = 0;
    glGetTranslatedShaderSourceANGLE(
        service_id_, max_len, &len, buffer.get());
    DCHECK(max_len == 0 || len < max_len);
    DCHECK(len == 0 || buffer[len] == '\0');
    translated_source_ = std::string(buffer.get(), len);
  }

  GLint status = GL_FALSE;
  glGetShaderiv(service_id_, GL_COMPILE_STATUS, &status);
  if (status != GL_TRUE) {
    // We cannot reach here if we are using the shader translator.
    // All invalid shaders must be rejected by the translator.
    // All translated shaders must compile.
    GLint max_len = 0;
    glGetShaderiv(service_id_, GL_INFO_LOG_LENGTH, &max_len);
    scoped_ptr<char[]> buffer(new char[max_len]);
    GLint len = 0;
    glGetShaderInfoLog(service_id_, max_len, &len, buffer.get());
    DCHECK(max_len == 0 || len < max_len);
    DCHECK(len == 0 || buffer[len] == '\0');
    valid_ = false;
    log_info_ = std::string(buffer.get(), len);
    LOG_IF(ERROR, translator)
        << "Shader translator allowed/produced an invalid shader "
        << "unless the driver is buggy:"
        << "\n--original-shader--\n" << source_
        << "\n--translated-shader--\n" << source_for_driver
        << "\n--info-log--\n" << log_info_;
  }
}

void Shader::IncUseCount() {
  ++use_count_;
}

void Shader::DecUseCount() {
  --use_count_;
  DCHECK_GE(use_count_, 0);
}

void Shader::MarkAsDeleted() {
  DCHECK_NE(service_id_, 0u);
  service_id_ = 0;
}

const Shader::VariableInfo* Shader::GetAttribInfo(
    const std::string& name) const {
  VariableMap::const_iterator it = attrib_map_.find(name);
  return it != attrib_map_.end() ? &it->second : NULL;
}

const std::string* Shader::GetAttribMappedName(
    const std::string& original_name) const {
  for (VariableMap::const_iterator it = attrib_map_.begin();
       it != attrib_map_.end(); ++it) {
    if (it->second.name == original_name)
      return &(it->first);
  }
  return NULL;
}

const std::string* Shader::GetOriginalNameFromHashedName(
    const std::string& hashed_name) const {
  NameMap::const_iterator it = name_map_.find(hashed_name);
  if (it != name_map_.end())
    return &(it->second);
  return NULL;
}

const Shader::VariableInfo* Shader::GetUniformInfo(
    const std::string& name) const {
  VariableMap::const_iterator it = uniform_map_.find(name);
  return it != uniform_map_.end() ? &it->second : NULL;
}

const Shader::VariableInfo* Shader::GetVaryingInfo(
    const std::string& name) const {
  VariableMap::const_iterator it = varying_map_.find(name);
  return it != varying_map_.end() ? &it->second : NULL;
}

ShaderManager::ShaderManager() {}

ShaderManager::~ShaderManager() {
  DCHECK(shaders_.empty());
}

void ShaderManager::Destroy(bool have_context) {
  while (!shaders_.empty()) {
    if (have_context) {
      Shader* shader = shaders_.begin()->second.get();
      if (!shader->IsDeleted()) {
        glDeleteShader(shader->service_id());
        shader->MarkAsDeleted();
      }
    }
    shaders_.erase(shaders_.begin());
  }
}

Shader* ShaderManager::CreateShader(
    GLuint client_id,
    GLuint service_id,
    GLenum shader_type) {
  std::pair<ShaderMap::iterator, bool> result =
      shaders_.insert(std::make_pair(
          client_id, scoped_refptr<Shader>(
              new Shader(service_id, shader_type))));
  DCHECK(result.second);
  return result.first->second.get();
}

Shader* ShaderManager::GetShader(GLuint client_id) {
  ShaderMap::iterator it = shaders_.find(client_id);
  return it != shaders_.end() ? it->second.get() : NULL;
}

bool ShaderManager::GetClientId(GLuint service_id, GLuint* client_id) const {
  // This doesn't need to be fast. It's only used during slow queries.
  for (ShaderMap::const_iterator it = shaders_.begin();
       it != shaders_.end(); ++it) {
    if (it->second->service_id() == service_id) {
      *client_id = it->first;
      return true;
    }
  }
  return false;
}

bool ShaderManager::IsOwned(Shader* shader) {
  for (ShaderMap::iterator it = shaders_.begin();
       it != shaders_.end(); ++it) {
    if (it->second.get() == shader) {
      return true;
    }
  }
  return false;
}

void ShaderManager::RemoveShader(Shader* shader) {
  DCHECK(shader);
  DCHECK(IsOwned(shader));
  if (shader->IsDeleted() && !shader->InUse()) {
    for (ShaderMap::iterator it = shaders_.begin();
         it != shaders_.end(); ++it) {
      if (it->second.get() == shader) {
        shaders_.erase(it);
        return;
      }
    }
    NOTREACHED();
  }
}

void ShaderManager::MarkAsDeleted(Shader* shader) {
  DCHECK(shader);
  DCHECK(IsOwned(shader));
  shader->MarkAsDeleted();
  RemoveShader(shader);
}

void ShaderManager::UseShader(Shader* shader) {
  DCHECK(shader);
  DCHECK(IsOwned(shader));
  shader->IncUseCount();
}

void ShaderManager::UnuseShader(Shader* shader) {
  DCHECK(shader);
  DCHECK(IsOwned(shader));
  shader->DecUseCount();
  RemoveShader(shader);
}

}  // namespace gles2
}  // namespace gpu


