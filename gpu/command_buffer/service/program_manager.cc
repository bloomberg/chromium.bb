// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/program_manager.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

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
  glGetProgramiv(program_id_, GL_ACTIVE_ATTRIBUTES, &num_attribs);
  attrib_infos_.resize(num_attribs);
  glGetProgramiv(program_id_, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &max_len);
  // TODO(gman): Should we check for error?
  scoped_array<char> name_buffer(new char[max_len]);
  for (GLint ii = 0; ii < num_attribs; ++ii) {
    GLsizei length;
    GLsizei size;
    GLenum type;
    glGetActiveAttrib(
        program_id_, ii, max_len, &length, &size, &type, name_buffer.get());
    // TODO(gman): Should we check for error?
    GLint location = IsInvalidPrefix(name_buffer.get(), length) ? -1 :
         glGetAttribLocation(program_id_, name_buffer.get());
    SetAttributeInfo(ii, size, type, location, name_buffer.get());
  }
  GLint num_uniforms;
  glGetProgramiv(program_id_, GL_ACTIVE_UNIFORMS, &num_uniforms);
  uniform_infos_.resize(num_uniforms);
  sampler_indices_.clear();
  glGetProgramiv(program_id_, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_len);
  name_buffer.reset(new char[max_len]);
  GLint max_location = -1;
  for (GLint ii = 0; ii < num_uniforms; ++ii) {
    GLsizei length;
    GLsizei size;
    GLenum type;
    glGetActiveUniform(
        program_id_, ii, max_len, &length, &size, &type, name_buffer.get());
    // TODO(gman): Should we check for error?
    GLint location = IsInvalidPrefix(name_buffer.get(), length) ? -1 :
        glGetUniformLocation(program_id_, name_buffer.get());
    SetUniformInfo(ii, size, type, location, name_buffer.get());
    const UniformInfo& info = uniform_infos_[ii];
    for (size_t jj = 0; jj < info.element_locations.size(); ++jj) {
      if (info.element_locations[jj] > max_location) {
        max_location = info.element_locations[jj];
      }
    }
    if (info.IsSampler()) {
      sampler_indices_.push_back(ii);
    }
  }
  // Create location to index map.
  location_to_index_map_.resize(max_location + 1);
  for (GLint ii = 0; ii <= max_location; ++ii) {
    location_to_index_map_[ii] = -1;
  }
  for (GLint ii = 0; ii < num_uniforms; ++ii) {
    const UniformInfo& info = uniform_infos_[ii];
    for (size_t jj = 0; jj < info.element_locations.size(); ++jj) {
      location_to_index_map_[info.element_locations[jj]] = ii;
    }
  }
}

GLint ProgramManager::ProgramInfo::GetUniformLocation(
    const std::string& name) const {
  for (GLuint ii = 0; ii < uniform_infos_.size(); ++ii) {
    const UniformInfo& info = uniform_infos_[ii];
    if (info.name == name ||
        (info.is_array &&
         info.name.compare(0, info.name.size() - 3, name) == 0)) {
      return info.element_locations[0];
    } else if (info.is_array &&
               name.size() >= 3 && name[name.size() - 1] == ']') {
      // Look for an array specification.
      size_t open_pos = name.find_last_of('[');
      if (open_pos != std::string::npos &&
          open_pos < name.size() - 2 &&
          info.name.size() > open_pos &&
          name.compare(0, open_pos, info.name, 0, open_pos) == 0) {
        GLint index = 0;
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
        if (!bad && index >= 0 && index < info.size) {
          return info.element_locations[index];
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
  if (location >= 0 &&
      static_cast<size_t>(location) < location_to_index_map_.size()) {
    GLint index = location_to_index_map_[location];
    if (index >= 0) {
      *type = uniform_infos_[index].type;
      return true;
    }
  }
  return false;
}

void ProgramManager::ProgramInfo::SetUniformInfo(
    GLint index, GLsizei size, GLenum type, GLint location,
    const std::string& name) {
  DCHECK(static_cast<unsigned>(index) < uniform_infos_.size());
  const char* kArraySpec = "[0]";
  UniformInfo& info = uniform_infos_[index];
  info.size = size;
  info.type = type;
  info.name = name;
  info.element_locations.resize(size);
  info.element_locations[0] = location;
  size_t num_texture_units = info.IsSampler() ? size : 0u;
  info.texture_units.clear();
  info.texture_units.resize(num_texture_units, 0);

  if (size > 1) {
    for (GLsizei ii = 1; ii < info.size; ++ii) {
      std::string element_name(name + "[" + IntToString(ii) + "]");
      info.element_locations[ii] =
          glGetUniformLocation(program_id_, element_name.c_str());
    }
    // Sadly there is no way to tell if this is an array except if the name
    // has an array string or the size > 1. That means an array of size 1 can
    // be ambiguous.
    //
    // For now we just make sure that if the size is > 1 then the name must have
    // an array spec.

    // Go through the array element locations looking for a match.
    // We can skip the first element because it's the same as the
    // the location without the array operators.
    size_t array_pos = name.rfind(kArraySpec);
    if (name.size() > 3 && array_pos != name.size() - 3) {
      info.name = name + kArraySpec;
    }
  }

  info.is_array =
     (size > 1 ||
      (info.name.size() > 3 &&
       info.name.rfind(kArraySpec) == info.name.size() - 3));
}

bool ProgramManager::ProgramInfo::SetSamplers(
    GLint location, GLsizei count, const GLint* value) {
  if (location >= 0 &&
      static_cast<size_t>(location) < location_to_index_map_.size()) {
    GLint index = location_to_index_map_[location];
    if (index >= 0) {
      UniformInfo& info = uniform_infos_[index];
      if (info.IsSampler() && count <= info.size) {
        std::copy(value, value + count, info.texture_units.begin());
        return true;
      }
    }
  }
  return false;
}

void ProgramManager::CreateProgramInfo(GLuint program_id) {
  std::pair<ProgramInfoMap::iterator, bool> result =
      program_infos_.insert(
          std::make_pair(program_id,
                         ProgramInfo::Ref(new ProgramInfo(program_id))));
  DCHECK(result.second);
}

ProgramManager::ProgramInfo* ProgramManager::GetProgramInfo(GLuint program_id) {
  ProgramInfoMap::iterator it = program_infos_.find(program_id);
  return it != program_infos_.end() ? it->second : NULL;
}

void ProgramManager::RemoveProgramInfo(GLuint program_id) {
  ProgramInfoMap::iterator it = program_infos_.find(program_id);
  if (it != program_infos_.end()) {
    it->second->MarkAsDeleted();
    program_infos_.erase(it);
  }
}

}  // namespace gles2
}  // namespace gpu


