// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/program_info_manager.h"

namespace {

template<typename T> static T LocalGetAs(
    const std::vector<int8>& data, uint32 offset, size_t size) {
  const int8* p = &data[0] + offset;
  if (offset + size > data.size()) {
    NOTREACHED();
    return NULL;
  }
  return static_cast<T>(static_cast<const void*>(p));
}

}  // namespace anonymous

namespace gpu {
namespace gles2 {

ProgramInfoManager::Program::VertexAttrib::VertexAttrib(
    GLsizei _size, GLenum _type, const std::string& _name, GLint _location)
    : size(_size),
      type(_type),
      location(_location),
      name(_name) {
}

ProgramInfoManager::Program::VertexAttrib::~VertexAttrib() {
}

ProgramInfoManager::Program::UniformInfo::UniformInfo(
    GLsizei _size, GLenum _type, const std::string& _name)
    : size(_size),
      type(_type),
      name(_name) {
  is_array = (!name.empty() && name[name.size() - 1] == ']');
  DCHECK(!(size > 1 && !is_array));
}

ProgramInfoManager::Program::UniformInfo::~UniformInfo() {
}

ProgramInfoManager::Program::Program()
    : cached_(false),
      max_attrib_name_length_(0),
      max_uniform_name_length_(0),
      link_status_(false) {
}

ProgramInfoManager::Program::~Program() {
}

// TODO(gman): Add a faster lookup.
GLint ProgramInfoManager::Program::GetAttribLocation(
    const std::string& name) const {
  for (GLuint ii = 0; ii < attrib_infos_.size(); ++ii) {
    const VertexAttrib& info = attrib_infos_[ii];
    if (info.name == name) {
      return info.location;
    }
  }
  return -1;
}

const ProgramInfoManager::Program::VertexAttrib*
ProgramInfoManager::Program::GetAttribInfo(GLint index) const {
  return (static_cast<size_t>(index) < attrib_infos_.size()) ?
      &attrib_infos_[index] : NULL;
}

const ProgramInfoManager::Program::UniformInfo*
ProgramInfoManager::Program::GetUniformInfo(GLint index) const {
  return (static_cast<size_t>(index) < uniform_infos_.size()) ?
      &uniform_infos_[index] : NULL;
}

GLint ProgramInfoManager::Program::GetUniformLocation(
    const std::string& name) const {
  bool getting_array_location = false;
  size_t open_pos = std::string::npos;
  int index = 0;
  if (!GLES2Util::ParseUniformName(
      name, &open_pos, &index, &getting_array_location)) {
    return -1;
  }
  for (GLuint ii = 0; ii < uniform_infos_.size(); ++ii) {
    const UniformInfo& info = uniform_infos_[ii];
    if (info.name == name ||
        (info.is_array &&
         info.name.compare(0, info.name.size() - 3, name) == 0)) {
      return info.element_locations[0];
    } else if (getting_array_location && info.is_array) {
      // Look for an array specification.
      size_t open_pos_2 = info.name.find_last_of('[');
      if (open_pos_2 == open_pos &&
          name.compare(0, open_pos, info.name, 0, open_pos) == 0) {
        if (index >= 0 && index < info.size) {
          return info.element_locations[index];
        }
      }
    }
  }
  return -1;
}

GLint ProgramInfoManager::Program::GetFragDataLocation(
    const std::string& name) const {
  base::hash_map<std::string, GLint>::const_iterator iter =
      frag_data_locations_.find(name);
  if (iter == frag_data_locations_.end())
    return -1;
  return iter->second;
}

void ProgramInfoManager::Program::CacheFragDataLocation(
    const std::string& name, GLint loc) {
  frag_data_locations_[name] = loc;
}

bool ProgramInfoManager::Program::GetProgramiv(
    GLenum pname, GLint* params) {
  switch (pname) {
    case GL_LINK_STATUS:
      *params = link_status_;
      return true;
    case GL_ACTIVE_ATTRIBUTES:
      *params = attrib_infos_.size();
      return true;
    case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:
      *params = max_attrib_name_length_;
      return true;
    case GL_ACTIVE_UNIFORMS:
      *params = uniform_infos_.size();
      return true;
    case GL_ACTIVE_UNIFORM_MAX_LENGTH:
      *params = max_uniform_name_length_;
      return true;
    default:
      break;
  }
  return false;
}

void ProgramInfoManager::Program::Update(
    GLES2Implementation* gl,
    GLuint program,
    const std::vector<int8>& result) {
  if (cached_) {
    return;
  }
  if (result.empty()) {
    // This should only happen on a lost context.
    return;
  }
  DCHECK_GE(result.size(), sizeof(ProgramInfoHeader));
  const ProgramInfoHeader* header = LocalGetAs<const ProgramInfoHeader*>(
      result, 0, sizeof(header));
  link_status_ = header->link_status != 0;
  if (!link_status_) {
    return;
  }
  attrib_infos_.clear();
  uniform_infos_.clear();
  frag_data_locations_.clear();
  max_attrib_name_length_ = 0;
  max_uniform_name_length_ = 0;
  const ProgramInput* inputs = LocalGetAs<const ProgramInput*>(
      result, sizeof(*header),
      sizeof(ProgramInput) * (header->num_attribs + header->num_uniforms));
  const ProgramInput* input = inputs;
  for (uint32 ii = 0; ii < header->num_attribs; ++ii) {
    const int32* location = LocalGetAs<const int32*>(
        result, input->location_offset, sizeof(int32));
    const char* name_buf = LocalGetAs<const char*>(
        result, input->name_offset, input->name_length);
    std::string name(name_buf, input->name_length);
    attrib_infos_.push_back(
        VertexAttrib(input->size, input->type, name, *location));
    max_attrib_name_length_ = std::max(
        static_cast<GLsizei>(name.size() + 1), max_attrib_name_length_);
    ++input;
  }
  for (uint32 ii = 0; ii < header->num_uniforms; ++ii) {
    const int32* locations = LocalGetAs<const int32*>(
        result, input->location_offset, sizeof(int32) * input->size);
    const char* name_buf = LocalGetAs<const char*>(
        result, input->name_offset, input->name_length);
    std::string name(name_buf, input->name_length);
    UniformInfo info(input->size, input->type, name);
    max_uniform_name_length_ = std::max(
        static_cast<GLsizei>(name.size() + 1), max_uniform_name_length_);
    for (int32 jj = 0; jj < input->size; ++jj) {
      info.element_locations.push_back(locations[jj]);
    }
    uniform_infos_.push_back(info);
    ++input;
  }
  DCHECK_EQ(header->num_attribs + header->num_uniforms,
                static_cast<uint32>(input - inputs));
  cached_ = true;
}

bool ProgramInfoManager::Program::cached() const {
  return cached_;
}


ProgramInfoManager::ProgramInfoManager() {
}

ProgramInfoManager::~ProgramInfoManager() {
}

ProgramInfoManager::Program* ProgramInfoManager::GetProgramInfo(
    GLES2Implementation* gl, GLuint program) {
  lock_.AssertAcquired();
  ProgramInfoMap::iterator it = program_infos_.find(program);
  if (it == program_infos_.end()) {
    return NULL;
  }
  Program* info = &it->second;
  if (info->cached())
    return info;
  std::vector<int8> result;
  {
    base::AutoUnlock unlock(lock_);
    // lock_ can't be held across IPC call or else it may deadlock in pepper.
    // http://crbug.com/418651
    gl->GetProgramInfoCHROMIUMHelper(program, &result);
  }

  it = program_infos_.find(program);
  if (it == program_infos_.end()) {
    return NULL;
  }
  info = &it->second;
  info->Update(gl, program, result);
  return info;
}

void ProgramInfoManager::CreateInfo(GLuint program) {
  base::AutoLock auto_lock(lock_);
  program_infos_.erase(program);
  std::pair<ProgramInfoMap::iterator, bool> result =
      program_infos_.insert(std::make_pair(program, Program()));

  DCHECK(result.second);
}

void ProgramInfoManager::DeleteInfo(GLuint program) {
  base::AutoLock auto_lock(lock_);
  program_infos_.erase(program);
}

bool ProgramInfoManager::GetProgramiv(
    GLES2Implementation* gl, GLuint program, GLenum pname, GLint* params) {
  base::AutoLock auto_lock(lock_);
  Program* info = GetProgramInfo(gl, program);
  if (!info) {
    return false;
  }
  return info->GetProgramiv(pname, params);
}

GLint ProgramInfoManager::GetAttribLocation(
    GLES2Implementation* gl, GLuint program, const char* name) {
  {
    base::AutoLock auto_lock(lock_);
    Program* info = GetProgramInfo(gl, program);
    if (info) {
      return info->GetAttribLocation(name);
    }
  }
  return gl->GetAttribLocationHelper(program, name);
}

GLint ProgramInfoManager::GetUniformLocation(
    GLES2Implementation* gl, GLuint program, const char* name) {
  {
    base::AutoLock auto_lock(lock_);
    Program* info = GetProgramInfo(gl, program);
    if (info) {
      return info->GetUniformLocation(name);
    }
  }
  return gl->GetUniformLocationHelper(program, name);
}

GLint ProgramInfoManager::GetFragDataLocation(
    GLES2Implementation* gl, GLuint program, const char* name) {
  // TODO(zmo): make FragData locations part of the ProgramInfo that are
  // fetched altogether from the service side.  See crbug.com/452104.
  {
    base::AutoLock auto_lock(lock_);
    Program* info = GetProgramInfo(gl, program);
    if (info) {
      GLint possible_loc = info->GetFragDataLocation(name);
      if (possible_loc != -1)
        return possible_loc;
    }
  }
  GLint loc = gl->GetFragDataLocationHelper(program, name);
  if (loc != -1) {
    base::AutoLock auto_lock(lock_);
    Program* info = GetProgramInfo(gl, program);
    if (info) {
      info->CacheFragDataLocation(name, loc);
    }
  }
  return loc;
}

bool ProgramInfoManager::GetActiveAttrib(
    GLES2Implementation* gl,
    GLuint program, GLuint index, GLsizei bufsize, GLsizei* length,
    GLint* size, GLenum* type, char* name) {
  {
    base::AutoLock auto_lock(lock_);
    Program* info = GetProgramInfo(gl, program);
    if (info) {
      const Program::VertexAttrib* attrib_info = info->GetAttribInfo(index);
      if (attrib_info) {
        if (size) {
          *size = attrib_info->size;
        }
        if (type) {
          *type = attrib_info->type;
        }
        if (length || name) {
          GLsizei max_size = std::min(
              static_cast<size_t>(bufsize) - 1,
              std::max(static_cast<size_t>(0), attrib_info->name.size()));
          if (length) {
            *length = max_size;
          }
          if (name && bufsize > 0) {
            memcpy(name, attrib_info->name.c_str(), max_size);
            name[max_size] = '\0';
          }
        }
        return true;
      }
    }
  }
  return gl->GetActiveAttribHelper(
      program, index, bufsize, length, size, type, name);
}

bool ProgramInfoManager::GetActiveUniform(
    GLES2Implementation* gl,
    GLuint program, GLuint index, GLsizei bufsize, GLsizei* length,
    GLint* size, GLenum* type, char* name) {
  {
    base::AutoLock auto_lock(lock_);
    Program* info = GetProgramInfo(gl, program);
    if (info) {
      const Program::UniformInfo* uniform_info = info->GetUniformInfo(index);
      if (uniform_info) {
        if (size) {
          *size = uniform_info->size;
        }
        if (type) {
          *type = uniform_info->type;
        }
        if (length || name) {
          GLsizei max_size = std::min(
              static_cast<size_t>(bufsize) - 1,
              std::max(static_cast<size_t>(0), uniform_info->name.size()));
          if (length) {
            *length = max_size;
          }
          if (name && bufsize > 0) {
            memcpy(name, uniform_info->name.c_str(), max_size);
            name[max_size] = '\0';
          }
        }
        return true;
      }
    }
  }
  return gl->GetActiveUniformHelper(
      program, index, bufsize, length, size, type, name);
}

}  // namespace gles2
}  // namespace gpu

