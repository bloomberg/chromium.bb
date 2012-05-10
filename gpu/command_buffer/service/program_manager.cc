// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/program_manager.h"

#include <algorithm>
#include <set>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gpu_switches.h"

namespace gpu {
namespace gles2 {

static int ShaderTypeToIndex(GLenum shader_type) {
  switch (shader_type) {
    case GL_VERTEX_SHADER:
      return 0;
    case GL_FRAGMENT_SHADER:
      return 1;
    default:
      NOTREACHED();
      return 0;
  }
}

ProgramManager::ProgramInfo::UniformInfo::UniformInfo(
    GLsizei _size,
    GLenum _type,
    int _fake_location_base,
    const std::string& _name)
    : size(_size),
      type(_type),
      fake_location_base(_fake_location_base),
      is_array(false),
      name(_name) {
}

ProgramManager::ProgramInfo::UniformInfo::~UniformInfo() {}

bool ProgramManager::IsInvalidPrefix(const char* name, size_t length) {
  static const char kInvalidPrefix[] = { 'g', 'l', '_' };
  return (length >= sizeof(kInvalidPrefix) &&
      memcmp(name, kInvalidPrefix, sizeof(kInvalidPrefix)) == 0);
}

ProgramManager::ProgramInfo::ProgramInfo(
    ProgramManager* manager, GLuint service_id)
    : manager_(manager),
      use_count_(0),
      max_attrib_name_length_(0),
      max_uniform_name_length_(0),
      service_id_(service_id),
      deleted_(false),
      valid_(false),
      link_status_(false),
      uniforms_cleared_(false) {
  manager_->StartTracking(this);
}

void ProgramManager::ProgramInfo::Reset() {
  valid_ = false;
  link_status_ = false;
  max_uniform_name_length_ = 0;
  max_attrib_name_length_ = 0;
  attrib_infos_.clear();
  uniform_infos_.clear();
  sampler_indices_.clear();
  attrib_location_to_index_map_.clear();
}

void ProgramManager::ProgramInfo::UpdateLogInfo() {
  GLint max_len = 0;
  glGetProgramiv(service_id_, GL_INFO_LOG_LENGTH, &max_len);
  if (max_len == 0) {
    set_log_info(NULL);
    return;
  }
  scoped_array<char> temp(new char[max_len]);
  GLint len = 0;
  glGetProgramInfoLog(service_id_, max_len, &len, temp.get());
  DCHECK(max_len == 0 || len < max_len);
  DCHECK(len == 0 || temp[len] == '\0');
  set_log_info(std::string(temp.get(), len).c_str());
}

void ProgramManager::ProgramInfo::ClearUniforms(
    std::vector<uint8>* zero_buffer) {
  DCHECK(zero_buffer);
  if (uniforms_cleared_) {
    return;
  }
  uniforms_cleared_ = true;
  for (size_t ii = 0; ii < uniform_infos_.size(); ++ii) {
    const UniformInfo& uniform_info = uniform_infos_[ii];
    GLint location = uniform_info.element_locations[0];
    GLsizei size = uniform_info.size;
    uint32 unit_size =  GLES2Util::GetGLDataTypeSizeForUniforms(
        uniform_info.type);
    uint32 size_needed = size * unit_size;
    if (size_needed > zero_buffer->size()) {
      zero_buffer->resize(size_needed, 0u);
    }
    const void* zero = &(*zero_buffer)[0];
    switch (uniform_info.type) {
    case GL_FLOAT:
      glUniform1fv(location, size, reinterpret_cast<const GLfloat*>(zero));
      break;
    case GL_FLOAT_VEC2:
      glUniform2fv(location, size, reinterpret_cast<const GLfloat*>(zero));
      break;
    case GL_FLOAT_VEC3:
      glUniform3fv(location, size, reinterpret_cast<const GLfloat*>(zero));
      break;
    case GL_FLOAT_VEC4:
      glUniform4fv(location, size, reinterpret_cast<const GLfloat*>(zero));
      break;
    case GL_INT:
    case GL_BOOL:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_CUBE:
    case GL_SAMPLER_EXTERNAL_OES:
    case GL_SAMPLER_3D_OES:
    case GL_SAMPLER_2D_RECT_ARB:
      glUniform1iv(location, size, reinterpret_cast<const GLint*>(zero));
      break;
    case GL_INT_VEC2:
    case GL_BOOL_VEC2:
      glUniform2iv(location, size, reinterpret_cast<const GLint*>(zero));
      break;
    case GL_INT_VEC3:
    case GL_BOOL_VEC3:
      glUniform3iv(location, size, reinterpret_cast<const GLint*>(zero));
      break;
    case GL_INT_VEC4:
    case GL_BOOL_VEC4:
      glUniform4iv(location, size, reinterpret_cast<const GLint*>(zero));
      break;
    case GL_FLOAT_MAT2:
      glUniformMatrix2fv(
          location, size, false, reinterpret_cast<const GLfloat*>(zero));
      break;
    case GL_FLOAT_MAT3:
      glUniformMatrix3fv(
          location, size, false, reinterpret_cast<const GLfloat*>(zero));
      break;
    case GL_FLOAT_MAT4:
      glUniformMatrix4fv(
          location, size, false, reinterpret_cast<const GLfloat*>(zero));
      break;
    default:
      NOTREACHED();
      break;
    }
  }
}

void ProgramManager::ProgramInfo::Update() {
  Reset();
  UpdateLogInfo();
  link_status_ = true;
  uniforms_cleared_ = false;
  GLint num_attribs = 0;
  GLint max_len = 0;
  GLint max_location = -1;
  glGetProgramiv(service_id_, GL_ACTIVE_ATTRIBUTES, &num_attribs);
  glGetProgramiv(service_id_, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &max_len);
  // TODO(gman): Should we check for error?
  scoped_array<char> name_buffer(new char[max_len]);
  for (GLint ii = 0; ii < num_attribs; ++ii) {
    GLsizei length = 0;
    GLsizei size = 0;
    GLenum type = 0;
    glGetActiveAttrib(
        service_id_, ii, max_len, &length, &size, &type, name_buffer.get());
    DCHECK(max_len == 0 || length < max_len);
    DCHECK(length == 0 || name_buffer[length] == '\0');
    if (!IsInvalidPrefix(name_buffer.get(), length)) {
      std::string name;
      std::string original_name;
      GetCorrectedVariableInfo(
          false, name_buffer.get(), &name, &original_name, &size, &type);
      // TODO(gman): Should we check for error?
      GLint location = glGetAttribLocation(service_id_, name_buffer.get());
      if (location > max_location) {
        max_location = location;
      }
      attrib_infos_.push_back(
          VertexAttribInfo(size, type, original_name, location));
      max_attrib_name_length_ = std::max(
          max_attrib_name_length_, static_cast<GLsizei>(original_name.size()));
    }
  }

  // Create attrib location to index map.
  attrib_location_to_index_map_.resize(max_location + 1);
  for (GLint ii = 0; ii <= max_location; ++ii) {
    attrib_location_to_index_map_[ii] = -1;
  }
  for (size_t ii = 0; ii < attrib_infos_.size(); ++ii) {
    const VertexAttribInfo& info = attrib_infos_[ii];
    attrib_location_to_index_map_[info.location] = ii;
  }

  max_len = 0;
  GLint num_uniforms = 0;
  glGetProgramiv(service_id_, GL_ACTIVE_UNIFORMS, &num_uniforms);
  glGetProgramiv(service_id_, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_len);
  name_buffer.reset(new char[max_len]);
  for (GLint ii = 0; ii < num_uniforms; ++ii) {
    GLsizei length = 0;
    GLsizei size = 0;
    GLenum type = 0;
    glGetActiveUniform(
        service_id_, ii, max_len, &length, &size, &type, name_buffer.get());
    DCHECK(max_len == 0 || length < max_len);
    DCHECK(length == 0 || name_buffer[length] == '\0');
    // TODO(gman): Should we check for error?
    if (!IsInvalidPrefix(name_buffer.get(), length)) {
      GLint location = glGetUniformLocation(service_id_, name_buffer.get());
      std::string name;
      std::string original_name;
      GetCorrectedVariableInfo(
          true, name_buffer.get(), &name, &original_name, &size, &type);
      const UniformInfo* info = AddUniformInfo(
          size, type, location, name, original_name);
      if (info->IsSampler()) {
        sampler_indices_.push_back(info->fake_location_base);
      }
      max_uniform_name_length_ =
          std::max(max_uniform_name_length_,
                   static_cast<GLsizei>(info->name.size()));
    }
  }
  valid_ = true;
}

void ProgramManager::ProgramInfo::ExecuteBindAttribLocationCalls() {
  for (std::map<std::string, GLint>::const_iterator it =
           bind_attrib_location_map_.begin();
       it != bind_attrib_location_map_.end(); ++it) {
    const std::string* mapped_name = GetAttribMappedName(it->first);
    if (mapped_name && *mapped_name != it->first)
      glBindAttribLocation(service_id_, it->second, mapped_name->c_str());
  }
}

bool ProgramManager::ProgramInfo::Link() {
  ClearLinkStatus();
  if (!CanLink()) {
    set_log_info("missing shaders");
    return false;
  }
  if (DetectAttribLocationBindingConflicts()) {
    set_log_info("glBindAttribLocation() conflicts");
    return false;
  }
  ExecuteBindAttribLocationCalls();
  glLinkProgram(service_id());
  GLint success = 0;
  glGetProgramiv(service_id(), GL_LINK_STATUS, &success);
  if (success == GL_TRUE) {
    Update();
  } else {
    UpdateLogInfo();
  }
  return success == GL_TRUE;
}

void ProgramManager::ProgramInfo::Validate() {
  if (!IsValid()) {
    set_log_info("program not linked");
    return;
  }
  glValidateProgram(service_id());
  UpdateLogInfo();
}

GLint ProgramManager::ProgramInfo::GetUniformFakeLocation(
    const std::string& name) const {
  for (GLuint ii = 0; ii < uniform_infos_.size(); ++ii) {
    const UniformInfo& info = uniform_infos_[ii];
    if (info.name == name ||
        (info.is_array &&
         info.name.compare(0, info.name.size() - 3, name) == 0)) {
      return info.fake_location_base;
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
          return GetFakeLocation(info.fake_location_base, index);
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

const ProgramManager::ProgramInfo::UniformInfo*
    ProgramManager::ProgramInfo::GetUniformInfoByFakeLocation(
        GLint fake_location, GLint* real_location, GLint* array_index) const {
  DCHECK(real_location);
  DCHECK(array_index);
  if (fake_location < 0) {
    return NULL;
  }

  GLint uniform_index = GetUniformInfoIndexFromFakeLocation(fake_location);
  if (uniform_index >= 0 &&
      static_cast<size_t>(uniform_index) < uniform_infos_.size()) {
    const UniformInfo& uniform_info = uniform_infos_[uniform_index];
    GLint element_index = GetArrayElementIndexFromFakeLocation(fake_location);
    if (element_index < uniform_info.size) {
      *real_location = uniform_info.element_locations[element_index];
      *array_index = element_index;
      return &uniform_info;
    }
  }
  return NULL;
}

const std::string* ProgramManager::ProgramInfo::GetAttribMappedName(
    const std::string& original_name) const {
  for (int ii = 0; ii < kMaxAttachedShaders; ++ii) {
    ShaderManager::ShaderInfo* shader_info = attached_shaders_[ii].get();
    if (shader_info) {
      const std::string* mapped_name =
          shader_info->GetAttribMappedName(original_name);
      if (mapped_name)
        return mapped_name;
    }
  }
  return NULL;
}

// Note: This is only valid to call right after a program has been linked
// successfully.
void ProgramManager::ProgramInfo::GetCorrectedVariableInfo(
    bool use_uniforms,
    const std::string& name, std::string* corrected_name,
    std::string* original_name,
    GLsizei* size, GLenum* type) const {
  DCHECK(corrected_name);
  DCHECK(original_name);
  DCHECK(size);
  DCHECK(type);
  const char* kArraySpec = "[0]";
  for (int jj = 0; jj < 2; ++jj) {
    std::string test_name(name + ((jj == 1) ? kArraySpec : ""));
    for (int ii = 0; ii < kMaxAttachedShaders; ++ii) {
      ShaderManager::ShaderInfo* shader_info = attached_shaders_[ii].get();
      if (shader_info) {
        const ShaderManager::ShaderInfo::VariableInfo* variable_info =
            use_uniforms ? shader_info->GetUniformInfo(test_name) :
                           shader_info->GetAttribInfo(test_name);
        // Note: There is an assuption here that if an attrib is defined in more
        // than 1 attached shader their types and sizes match. Should we check
        // for that case?
        if (variable_info) {
          *corrected_name = test_name;
          *original_name = variable_info->name;
          *type = variable_info->type;
          *size = variable_info->size;
          return;
        }
      }
    }
  }
  *corrected_name = name;
  *original_name = name;
}

const ProgramManager::ProgramInfo::UniformInfo*
    ProgramManager::ProgramInfo::AddUniformInfo(
        GLsizei size, GLenum type, GLint location,
        const std::string& name, const std::string& original_name) {
  const char* kArraySpec = "[0]";
  int uniform_index = uniform_infos_.size();
  uniform_infos_.push_back(
      UniformInfo(size, type, uniform_index, original_name));
  UniformInfo& info = uniform_infos_.back();
  info.element_locations.resize(size);
  info.element_locations[0] = location;
  DCHECK_GE(size, 0);
  size_t num_texture_units = info.IsSampler() ? static_cast<size_t>(size) : 0u;
  info.texture_units.clear();
  info.texture_units.resize(num_texture_units, 0);

  if (size > 1) {
    // Go through the array element locations looking for a match.
    // We can skip the first element because it's the same as the
    // the location without the array operators.
    size_t array_pos = name.rfind(kArraySpec);
    std::string base_name = name;
    if (name.size() > 3) {
      if (array_pos != name.size() - 3) {
        info.name = name + kArraySpec;
      } else {
        base_name = name.substr(0, name.size() - 3);
      }
    }
    for (GLsizei ii = 1; ii < info.size; ++ii) {
      std::string element_name(base_name + "[" + base::IntToString(ii) + "]");
      info.element_locations[ii] =
          glGetUniformLocation(service_id_, element_name.c_str());
    }
  }

  info.is_array =
     (size > 1 ||
      (info.name.size() > 3 &&
       info.name.rfind(kArraySpec) == info.name.size() - 3));

  return &info;
}

bool ProgramManager::ProgramInfo::SetSamplers(
    GLint num_texture_units, GLint fake_location,
    GLsizei count, const GLint* value) {
  if (fake_location < 0) {
    return true;
  }
  GLint uniform_index = GetUniformInfoIndexFromFakeLocation(fake_location);
  if (uniform_index >= 0 &&
      static_cast<size_t>(uniform_index) < uniform_infos_.size()) {
    UniformInfo& info = uniform_infos_[uniform_index];
    GLint element_index = GetArrayElementIndexFromFakeLocation(fake_location);
    if (element_index < info.size) {
      count = std::min(info.size - element_index, count);
      if (info.IsSampler() && count > 0) {
        for (GLsizei ii = 0; ii < count; ++ii) {
          if (value[ii] < 0 || value[ii] >= num_texture_units) {
            return false;
          }
        }
        std::copy(value, value + count,
                  info.texture_units.begin() + element_index);
        return true;
      }
    }
  }
  return true;
}

void ProgramManager::ProgramInfo::GetProgramiv(GLenum pname, GLint* params) {
  switch (pname) {
    case GL_ACTIVE_ATTRIBUTES:
      *params = attrib_infos_.size();
      break;
    case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:
      // Notice +1 to accomodate NULL terminator.
      *params = max_attrib_name_length_ + 1;
      break;
    case GL_ACTIVE_UNIFORMS:
      *params = uniform_infos_.size();
      break;
    case GL_ACTIVE_UNIFORM_MAX_LENGTH:
      // Notice +1 to accomodate NULL terminator.
      *params = max_uniform_name_length_ + 1;
      break;
    case GL_LINK_STATUS:
      *params = link_status_;
      break;
    case GL_INFO_LOG_LENGTH:
      // Notice +1 to accomodate NULL terminator.
      *params = log_info_.get() ? (log_info_->size() + 1) : 0;
      break;
    case GL_DELETE_STATUS:
      *params = deleted_;
      break;
    case GL_VALIDATE_STATUS:
      if (!IsValid()) {
        *params = GL_FALSE;
      } else {
        glGetProgramiv(service_id_, pname, params);
      }
      break;
    default:
      glGetProgramiv(service_id_, pname, params);
      break;
  }
}

bool ProgramManager::ProgramInfo::AttachShader(
    ShaderManager* shader_manager,
    ShaderManager::ShaderInfo* info) {
  DCHECK(shader_manager);
  DCHECK(info);
  int index = ShaderTypeToIndex(info->shader_type());
  if (attached_shaders_[index] != NULL) {
    return false;
  }
  attached_shaders_[index] = ShaderManager::ShaderInfo::Ref(info);
  shader_manager->UseShader(info);
  return true;
}

bool ProgramManager::ProgramInfo::DetachShader(
    ShaderManager* shader_manager,
    ShaderManager::ShaderInfo* info) {
  DCHECK(shader_manager);
  DCHECK(info);
  if (attached_shaders_[ShaderTypeToIndex(info->shader_type())].get() != info) {
    return false;
  }
  attached_shaders_[ShaderTypeToIndex(info->shader_type())] = NULL;
  shader_manager->UnuseShader(info);
  return true;
}

void ProgramManager::ProgramInfo::DetachShaders(ShaderManager* shader_manager) {
  DCHECK(shader_manager);
  for (int ii = 0; ii < kMaxAttachedShaders; ++ii) {
    if (attached_shaders_[ii]) {
      DetachShader(shader_manager, attached_shaders_[ii]);
    }
  }
}

bool ProgramManager::ProgramInfo::CanLink() const {
  for (int ii = 0; ii < kMaxAttachedShaders; ++ii) {
    if (!attached_shaders_[ii] || !attached_shaders_[ii]->IsValid()) {
      return false;
    }
  }
  return true;
}

bool ProgramManager::ProgramInfo::DetectAttribLocationBindingConflicts() const {
  std::set<GLint> location_binding_used;
  for (std::map<std::string, GLint>::const_iterator it =
           bind_attrib_location_map_.begin();
       it != bind_attrib_location_map_.end(); ++it) {
    // Find out if an attribute is declared in this program's shaders.
    bool active = false;
    for (int ii = 0; ii < kMaxAttachedShaders; ++ii) {
      if (!attached_shaders_[ii] || !attached_shaders_[ii]->IsValid())
        continue;
      if (attached_shaders_[ii]->GetAttribInfo(it->first)) {
        active = true;
        break;
      }
    }
    if (active) {
      std::pair<std::set<GLint>::iterator, bool> result =
          location_binding_used.insert(it->second);
      if (!result.second)
        return true;
    }
  }
  return false;
}

static uint32 ComputeOffset(const void* start, const void* position) {
  return static_cast<const uint8*>(position) -
         static_cast<const uint8*>(start);
}

void ProgramManager::ProgramInfo::GetProgramInfo(
    ProgramManager* manager, CommonDecoder::Bucket* bucket) const {
  // NOTE: It seems to me the math in here does not need check for overflow
  // because the data being calucated from has various small limits. The max
  // number of attribs + uniforms is somewhere well under 1024. The maximum size
  // of an identifier is 256 characters.
  uint32 num_locations = 0;
  uint32 total_string_size = 0;

  for (size_t ii = 0; ii < attrib_infos_.size(); ++ii) {
    const VertexAttribInfo& info = attrib_infos_[ii];
    num_locations += 1;
    total_string_size += info.name.size();
  }

  for (size_t ii = 0; ii < uniform_infos_.size(); ++ii) {
    const UniformInfo& info = uniform_infos_[ii];
    num_locations += info.element_locations.size();
    total_string_size += info.name.size();
  }

  uint32 num_inputs = attrib_infos_.size() + uniform_infos_.size();
  uint32 input_size = num_inputs * sizeof(ProgramInput);
  uint32 location_size = num_locations * sizeof(int32);
  uint32 size = sizeof(ProgramInfoHeader) +
      input_size + location_size + total_string_size;

  bucket->SetSize(size);
  ProgramInfoHeader* header = bucket->GetDataAs<ProgramInfoHeader*>(0, size);
  ProgramInput* inputs = bucket->GetDataAs<ProgramInput*>(
      sizeof(ProgramInfoHeader), input_size);
  int32* locations = bucket->GetDataAs<int32*>(
      sizeof(ProgramInfoHeader) + input_size, location_size);
  char* strings = bucket->GetDataAs<char*>(
      sizeof(ProgramInfoHeader) + input_size + location_size,
      total_string_size);
  DCHECK(header);
  DCHECK(inputs);
  DCHECK(locations);
  DCHECK(strings);

  header->link_status = link_status_;
  header->num_attribs = attrib_infos_.size();
  header->num_uniforms = uniform_infos_.size();

  for (size_t ii = 0; ii < attrib_infos_.size(); ++ii) {
    const VertexAttribInfo& info = attrib_infos_[ii];
    inputs->size = info.size;
    inputs->type = info.type;
    inputs->location_offset = ComputeOffset(header, locations);
    inputs->name_offset = ComputeOffset(header, strings);
    inputs->name_length = info.name.size();
    *locations++ = info.location;
    memcpy(strings, info.name.c_str(), info.name.size());
    strings += info.name.size();
    ++inputs;
  }

  for (size_t ii = 0; ii < uniform_infos_.size(); ++ii) {
    const UniformInfo& info = uniform_infos_[ii];
    inputs->size = info.size;
    inputs->type = info.type;
    inputs->location_offset = ComputeOffset(header, locations);
    inputs->name_offset = ComputeOffset(header, strings);
    inputs->name_length = info.name.size();
    DCHECK(static_cast<size_t>(info.size) == info.element_locations.size());
    for (size_t jj = 0; jj < info.element_locations.size(); ++jj) {
      *locations++ = manager->SwizzleLocation(ii + jj * 0x10000);
    }
    memcpy(strings, info.name.c_str(), info.name.size());
    strings += info.name.size();
    ++inputs;
  }

  DCHECK_EQ(ComputeOffset(header, strings), size);
}

ProgramManager::ProgramInfo::~ProgramInfo() {
  if (manager_) {
    if (manager_->have_context_) {
      glDeleteProgram(service_id());
    }
    manager_->StopTracking(this);
    manager_ = NULL;
  }
}

// TODO(gman): make this some kind of random number. Base::RandInt is not
// callable because of the sandbox. What matters is that it's possibly different
// by at least 1 bit each time chrome is run.
static int uniform_random_offset_ = 3;

ProgramManager::ProgramManager()
    : uniform_swizzle_(uniform_random_offset_++ % 15),
      program_info_count_(0),
      have_context_(true),
      disable_workarounds_(
          CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kDisableGpuDriverBugWorkarounds)) {
}

ProgramManager::~ProgramManager() {
  DCHECK(program_infos_.empty());
}

void ProgramManager::Destroy(bool have_context) {
  have_context_ = have_context;
  program_infos_.clear();
}

void ProgramManager::StartTracking(ProgramManager::ProgramInfo* /* program */) {
  ++program_info_count_;
}

void ProgramManager::StopTracking(ProgramManager::ProgramInfo* /* program */) {
  --program_info_count_;
}

ProgramManager::ProgramInfo* ProgramManager::CreateProgramInfo(
    GLuint client_id, GLuint service_id) {
  std::pair<ProgramInfoMap::iterator, bool> result =
      program_infos_.insert(
          std::make_pair(client_id,
                         ProgramInfo::Ref(new ProgramInfo(this, service_id))));
  DCHECK(result.second);
  return result.first->second;
}

ProgramManager::ProgramInfo* ProgramManager::GetProgramInfo(GLuint client_id) {
  ProgramInfoMap::iterator it = program_infos_.find(client_id);
  return it != program_infos_.end() ? it->second : NULL;
}

bool ProgramManager::GetClientId(GLuint service_id, GLuint* client_id) const {
  // This doesn't need to be fast. It's only used during slow queries.
  for (ProgramInfoMap::const_iterator it = program_infos_.begin();
       it != program_infos_.end(); ++it) {
    if (it->second->service_id() == service_id) {
      *client_id = it->first;
      return true;
    }
  }
  return false;
}

bool ProgramManager::IsOwned(ProgramManager::ProgramInfo* info) {
  for (ProgramInfoMap::iterator it = program_infos_.begin();
       it != program_infos_.end(); ++it) {
    if (it->second.get() == info) {
      return true;
    }
  }
  return false;
}

void ProgramManager::RemoveProgramInfoIfUnused(
    ShaderManager* shader_manager, ProgramInfo* info) {
  DCHECK(shader_manager);
  DCHECK(info);
  DCHECK(IsOwned(info));
  if (info->IsDeleted() && !info->InUse()) {
    info->DetachShaders(shader_manager);
    for (ProgramInfoMap::iterator it = program_infos_.begin();
         it != program_infos_.end(); ++it) {
      if (it->second.get() == info) {
        program_infos_.erase(it);
        return;
      }
    }
    NOTREACHED();
  }
}

void ProgramManager::MarkAsDeleted(
    ShaderManager* shader_manager,
    ProgramManager::ProgramInfo* info) {
  DCHECK(shader_manager);
  DCHECK(info);
  DCHECK(IsOwned(info));
  info->MarkAsDeleted();
  RemoveProgramInfoIfUnused(shader_manager, info);
}

void ProgramManager::UseProgram(ProgramManager::ProgramInfo* info) {
  DCHECK(info);
  DCHECK(IsOwned(info));
  info->IncUseCount();
  ClearUniforms(info);
}

void ProgramManager::UnuseProgram(
    ShaderManager* shader_manager,
    ProgramManager::ProgramInfo* info) {
  DCHECK(shader_manager);
  DCHECK(info);
  DCHECK(IsOwned(info));
  info->DecUseCount();
  RemoveProgramInfoIfUnused(shader_manager, info);
}

void ProgramManager::ClearUniforms(ProgramManager::ProgramInfo* info) {
  DCHECK(info);
  if (!disable_workarounds_) {
    info->ClearUniforms(&zero_);
  }
}

// Swizzles the locations to prevent developers from assuming they
// can do math on uniforms. According to the OpenGL ES 2.0 spec
// the location of "someuniform[1]" is not 'n' more than "someuniform[0]".
static GLint Swizzle(GLint location) {
  return (location & 0xF0000000U) |
         ((location & 0x0AAAAAAAU) >> 1) |
         ((location & 0x05555555U) << 1);
}

// Adds uniform_swizzle_ to prevent developers from assuming that locations are
// always the same across GPUs and drivers.
GLint ProgramManager::SwizzleLocation(GLint v) const {
  return v < 0 ? v : (Swizzle(v) + uniform_swizzle_);
}

GLint ProgramManager::UnswizzleLocation(GLint v) const {
  return v < 0 ? v : Swizzle(v - uniform_swizzle_);
}

}  // namespace gles2
}  // namespace gpu


