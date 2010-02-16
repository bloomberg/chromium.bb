// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/program_manager.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"

namespace gpu {
namespace gles2 {

bool ProgramManager::IsInvalidPrefix(const char* name, size_t length) {
  static const char kInvalidPrefix[] = { 'g', 'l', '_' };
  return (length >= sizeof(kInvalidPrefix) &&
      memcmp(name, kInvalidPrefix, sizeof(kInvalidPrefix)) == 0);
}

void ProgramManager::ProgramInfo::Update() {
  GLint num_attribs = 0;
  GLint max_len = 0;
  glGetProgramiv(program_, GL_ACTIVE_ATTRIBUTES, &num_attribs);
  SetNumAttributes(num_attribs);
  glGetProgramiv(program_, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &max_len);
  // TODO(gman): Should we check for error?
  scoped_array<char> name_buffer(new char[max_len]);
  for (GLint ii = 0; ii < num_attribs; ++ii) {
    GLsizei length;
    GLsizei size;
    GLenum type;
    glGetActiveAttrib(
        program_, ii, max_len, &length, &size, &type, name_buffer.get());
    // TODO(gman): Should we check for error?
    GLint location = IsInvalidPrefix(name_buffer.get(), length) ? -1 :
         glGetAttribLocation(program_, name_buffer.get());
    SetAttributeInfo(ii, size, type, location, name_buffer.get());
  }
  GLint num_uniforms;
  glGetProgramiv(program_, GL_ACTIVE_UNIFORMS, &num_uniforms);
  SetNumUniforms(num_uniforms);
  glGetProgramiv(program_, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_len);
  name_buffer.reset(new char[max_len]);
  for (GLint ii = 0; ii < num_uniforms; ++ii) {
    GLsizei length;
    GLsizei size;
    GLenum type;
    glGetActiveUniform(
        program_, ii, max_len, &length, &size, &type, name_buffer.get());
    // TODO(gman): Should we check for error?
    GLint location = IsInvalidPrefix(name_buffer.get(), length) ? -1 :
        glGetUniformLocation(program_, name_buffer.get());
    SetUniformInfo(ii, size, type, location, name_buffer.get());
  }
}

GLint ProgramManager::ProgramInfo::GetUniformLocation(const std::string& name) {
  for (GLuint ii = 0; ii < uniform_infos_.size(); ++ii) {
    const UniformInfo& info = uniform_infos_[ii];
    if (info.name == name) {
      return info.element_locations[0];
    } else if (name.size() >= 3 && name[name.size() - 1] == ']') {
      // Look for an array specfication.
      size_t open_pos = name.find_last_of('[');
      if (open_pos != std::string::npos && open_pos < name.size() - 2) {
        GLuint index = 0;
        size_t last = name.size() - 1;
        bool bad = false;
        for (size_t pos = open_pos + 1; pos < last; ++pos) {
          int8 digit = name[pos] - '0';
          if (digit < 0 || digit > 9) {
            bad = true;
            break;
          }
          index = index * 10 + digit;
        }
        if (!bad) {
          return index;
        }
      }
    }
  }
  return -1;
}

GLint ProgramManager::ProgramInfo::GetAttribLocation(
    const std::string& name) const {
  for (GLuint ii = 0; ii < attrib_infos_.size(); ++ii) {
    const VertexAttribInfo& info = attrib_infos_[ii];
    if (info.name == name) {
      return info.location;
    }
  }
  return -1;
}

bool ProgramManager::ProgramInfo::GetUniformTypeByLocation(
    GLint location, GLenum* type) const {
  for (GLuint ii = 0; ii < uniform_infos_.size(); ++ii) {
    const UniformInfo& info = uniform_infos_[ii];
    for (GLsizei jj = 0; jj < info.size; ++jj) {
      if (info.element_locations[jj] == location) {
        *type = info.type;
        return true;
      }
    }
  }
  return false;
}

void ProgramManager::ProgramInfo::SetUniformInfo(
    GLint index, GLsizei size, GLenum type, GLint location,
    const std::string& name) {
  DCHECK(static_cast<unsigned>(index) < uniform_infos_.size());
  UniformInfo& info = uniform_infos_[index];
  info.size = size;
  info.type = type;
  info.name = name;
  info.element_locations.resize(size);
  info.element_locations[0] = location;
  // Go through the array element locations looking for a match.
  // We can skip the first element because it's the same as the
  // the location without the array operators.
  if (size > 1) {
    for (GLsizei ii = 1; ii < info.size; ++ii) {
      std::string element_name(name + "[" + IntToString(ii) + "]");
      info.element_locations[ii] =
          glGetUniformLocation(program_, element_name.c_str());
    }
  }
}

void ProgramManager::CreateProgramInfo(GLuint program) {
  std::pair<ProgramInfoMap::iterator, bool> result =
      program_infos_.insert(
          std::make_pair(program, ProgramInfo(program)));
  DCHECK(result.second);
}

ProgramManager::ProgramInfo* ProgramManager::GetProgramInfo(GLuint program) {
  ProgramInfoMap::iterator it = program_infos_.find(program);
  return it != program_infos_.end() ? &it->second : NULL;
}

void ProgramManager::RemoveProgramInfo(GLuint program) {
  ProgramInfoMap::iterator it = program_infos_.find(program);
  if (it != program_infos_.end()) {
    program_infos_.erase(it);
  }
}

}  // namespace gles2
}  // namespace gpu


