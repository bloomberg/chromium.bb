// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/program_manager.h"

#include <algorithm>
#include <set>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/program_cache.h"
#include "third_party/re2/re2/re2.h"

using base::TimeDelta;
using base::TimeTicks;

namespace gpu {
namespace gles2 {

namespace {

int ShaderTypeToIndex(GLenum shader_type) {
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

ShaderTranslator* ShaderIndexToTranslator(
    int index,
    ShaderTranslator* vertex_translator,
    ShaderTranslator* fragment_translator) {
  switch (index) {
    case 0:
      return vertex_translator;
    case 1:
      return fragment_translator;
    default:
      NOTREACHED();
      return NULL;
  }
}

// Given a name like "foo.bar[123].moo[456]" sets new_name to "foo.bar[123].moo"
// and sets element_index to 456. returns false if element expression was not a
// whole decimal number. For example: "foo[1b2]"
bool GetUniformNameSansElement(
    const std::string& name, int* element_index, std::string* new_name) {
  DCHECK(element_index);
  DCHECK(new_name);
  if (name.size() < 3 || name[name.size() - 1] != ']') {
    *element_index = 0;
    *new_name = name;
    return true;
  }

  // Look for an array specification.
  size_t open_pos = name.find_last_of('[');
  if (open_pos == std::string::npos ||
      open_pos >= name.size() - 2) {
    return false;
  }

  GLint index = 0;
  size_t last = name.size() - 1;
  for (size_t pos = open_pos + 1; pos < last; ++pos) {
    int8 digit = name[pos] - '0';
    if (digit < 0 || digit > 9) {
      return false;
    }
    index = index * 10 + digit;
  }

  *element_index = index;
  *new_name = name.substr(0, open_pos);
  return true;
}

}  // anonymous namespace.

Program::UniformInfo::UniformInfo()
    : size(0),
      type(GL_NONE),
      fake_location_base(0),
      is_array(false) {
}

Program::UniformInfo::UniformInfo(
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

Program::UniformInfo::~UniformInfo() {}

bool ProgramManager::IsInvalidPrefix(const char* name, size_t length) {
  static const char kInvalidPrefix[] = { 'g', 'l', '_' };
  return (length >= sizeof(kInvalidPrefix) &&
      memcmp(name, kInvalidPrefix, sizeof(kInvalidPrefix)) == 0);
}

Program::Program(
    ProgramManager* manager, GLuint service_id)
    : manager_(manager),
      use_count_(0),
      max_attrib_name_length_(0),
      max_uniform_name_length_(0),
      service_id_(service_id),
      deleted_(false),
      valid_(false),
      link_status_(false),
      uniforms_cleared_(false),
      num_uniforms_(0) {
  manager_->StartTracking(this);
}

void Program::Reset() {
  valid_ = false;
  link_status_ = false;
  num_uniforms_ = 0;
  max_uniform_name_length_ = 0;
  max_attrib_name_length_ = 0;
  attrib_infos_.clear();
  uniform_infos_.clear();
  sampler_indices_.clear();
  attrib_location_to_index_map_.clear();
}

std::string Program::ProcessLogInfo(
    const std::string& log) {
  std::string output;
  re2::StringPiece input(log);
  std::string prior_log;
  std::string hashed_name;
  while (RE2::Consume(&input,
                      "(.*)(webgl_[0123456789abcdefABCDEF]+)",
                      &prior_log,
                      &hashed_name)) {
    output += prior_log;

    const std::string* original_name =
        GetOriginalNameFromHashedName(hashed_name);
    if (original_name)
      output += *original_name;
    else
      output += hashed_name;
  }

  return output + input.as_string();
}

void Program::UpdateLogInfo() {
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
  std::string log(temp.get(), len);
  set_log_info(ProcessLogInfo(log).c_str());
}

void Program::ClearUniforms(
    std::vector<uint8>* zero_buffer) {
  DCHECK(zero_buffer);
  if (uniforms_cleared_) {
    return;
  }
  uniforms_cleared_ = true;
  for (size_t ii = 0; ii < uniform_infos_.size(); ++ii) {
    const UniformInfo& uniform_info = uniform_infos_[ii];
    if (!uniform_info.IsValid()) {
      continue;
    }
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

namespace {

struct UniformData {
  UniformData() : size(-1), type(GL_NONE), location(0), added(false) {
  }
  std::string queried_name;
  std::string corrected_name;
  std::string original_name;
  GLsizei size;
  GLenum type;
  GLint location;
  bool added;
};

struct UniformDataComparer {
  bool operator()(const UniformData& lhs, const UniformData& rhs) const {
    return lhs.queried_name < rhs.queried_name;
  }
};

}  // anonymous namespace

void Program::Update() {
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
    if (!ProgramManager::IsInvalidPrefix(name_buffer.get(), length)) {
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
          VertexAttrib(size, type, original_name, location));
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
    const VertexAttrib& info = attrib_infos_[ii];
    attrib_location_to_index_map_[info.location] = ii;
  }

#if !defined(NDEBUG)
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableGPUServiceLoggingGPU)) {
    DLOG(INFO) << "----: attribs for service_id: " << service_id();
    for (size_t ii = 0; ii < attrib_infos_.size(); ++ii) {
      const VertexAttrib& info = attrib_infos_[ii];
      DLOG(INFO) << ii << ": loc = " << info.location
                 << ", size = " << info.size
                 << ", type = " << GLES2Util::GetStringEnum(info.type)
                 << ", name = " << info.name;
    }
  }
#endif

  max_len = 0;
  GLint num_uniforms = 0;
  glGetProgramiv(service_id_, GL_ACTIVE_UNIFORMS, &num_uniforms);
  glGetProgramiv(service_id_, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_len);
  name_buffer.reset(new char[max_len]);

  // Reads all the names.
  std::vector<UniformData> uniform_data_;
  for (GLint ii = 0; ii < num_uniforms; ++ii) {
    GLsizei length = 0;
    UniformData data;
    glGetActiveUniform(
        service_id_, ii, max_len, &length,
        &data.size, &data.type, name_buffer.get());
    DCHECK(max_len == 0 || length < max_len);
    DCHECK(length == 0 || name_buffer[length] == '\0');
    if (!ProgramManager::IsInvalidPrefix(name_buffer.get(), length)) {
      data.queried_name = std::string(name_buffer.get());
      GetCorrectedVariableInfo(
          true, name_buffer.get(), &data.corrected_name, &data.original_name,
          &data.size, &data.type);
      uniform_data_.push_back(data);
    }
  }

  // NOTE: We don't care if 2 uniforms are bound to the same location.
  // One of them will take preference. The spec allows this, same as
  // BindAttribLocation.
  //
  // The reason we don't check is if we were to fail we'd have to
  // restore the previous program but since we've already linked successfully
  // at this point the previous program is gone.

  // Assigns the uniforms with bindings.
  size_t next_available_index = 0;
  for (size_t ii = 0; ii < uniform_data_.size(); ++ii) {
    UniformData& data = uniform_data_[ii];
    data.location = glGetUniformLocation(
        service_id_, data.queried_name.c_str());
    // remove "[0]"
    std::string short_name;
    int element_index = 0;
    bool good ALLOW_UNUSED = GetUniformNameSansElement(
        data.queried_name, &element_index, &short_name);\
    DCHECK(good);
    LocationMap::const_iterator it = bind_uniform_location_map_.find(
        short_name);
    if (it != bind_uniform_location_map_.end()) {
      data.added = AddUniformInfo(
          data.size, data.type, data.location, it->second, data.corrected_name,
          data.original_name, &next_available_index);
    }
  }

  // Assigns the uniforms that were not bound.
  for (size_t ii = 0; ii < uniform_data_.size(); ++ii) {
    const UniformData& data = uniform_data_[ii];
    if (!data.added) {
      AddUniformInfo(
          data.size, data.type, data.location, -1, data.corrected_name,
          data.original_name, &next_available_index);
    }
  }

#if !defined(NDEBUG)
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableGPUServiceLoggingGPU)) {
    DLOG(INFO) << "----: uniforms for service_id: " << service_id();
    for (size_t ii = 0; ii < uniform_infos_.size(); ++ii) {
      const UniformInfo& info = uniform_infos_[ii];
      if (info.IsValid()) {
        DLOG(INFO) << ii << ": loc = " << info.element_locations[0]
                   << ", size = " << info.size
                   << ", type = " << GLES2Util::GetStringEnum(info.type)
                   << ", name = " << info.name;
      }
    }
  }
#endif

  valid_ = true;
}

void Program::ExecuteBindAttribLocationCalls() {
  for (LocationMap::const_iterator it = bind_attrib_location_map_.begin();
       it != bind_attrib_location_map_.end(); ++it) {
    const std::string* mapped_name = GetAttribMappedName(it->first);
    if (mapped_name && *mapped_name != it->first)
      glBindAttribLocation(service_id_, it->second, mapped_name->c_str());
  }
}

void ProgramManager::DoCompileShader(Shader* info,
                                     ShaderTranslator* translator,
                                     FeatureInfo* feature_info) {
  TimeTicks before = TimeTicks::HighResNow();
  if (program_cache_ &&
      program_cache_->GetShaderCompilationStatus(info->source() ?
                                                 *info->source() : "") ==
          ProgramCache::COMPILATION_SUCCEEDED) {
    info->SetStatus(true, "", translator);
    info->FlagSourceAsCompiled(false);
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "GPU.ProgramCache.CompilationCacheHitTime",
        (TimeTicks::HighResNow() - before).InMicroseconds(),
        0,
        TimeDelta::FromSeconds(1).InMicroseconds(),
        50);
    return;
  }
  ForceCompileShader(info->source(), info, translator, feature_info);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "GPU.ProgramCache.CompilationCacheMissTime",
      (TimeTicks::HighResNow() - before).InMicroseconds(),
      0,
      TimeDelta::FromSeconds(1).InMicroseconds(),
      50);
}

void ProgramManager::ForceCompileShader(const std::string* source,
                                        Shader* info,
                                        ShaderTranslator* translator,
                                        FeatureInfo* feature_info) {
  info->FlagSourceAsCompiled(true);

  // Translate GL ES 2.0 shader to Desktop GL shader and pass that to
  // glShaderSource and then glCompileShader.
  const char* shader_src = source ? source->c_str() : "";
  if (translator) {
    if (!translator->Translate(shader_src)) {
      info->SetStatus(false, translator->info_log(), NULL);
      return;
    }
    shader_src = translator->translated_shader();
    if (!feature_info->feature_flags().angle_translated_shader_source)
      info->UpdateTranslatedSource(shader_src);
  }

  glShaderSource(info->service_id(), 1, &shader_src, NULL);
  glCompileShader(info->service_id());
  if (feature_info->feature_flags().angle_translated_shader_source) {
    GLint max_len = 0;
    glGetShaderiv(info->service_id(),
                  GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE,
                  &max_len);
    scoped_array<char> temp(new char[max_len]);
    GLint len = 0;
    glGetTranslatedShaderSourceANGLE(
        info->service_id(), max_len, &len, temp.get());
    DCHECK(max_len == 0 || len < max_len);
    DCHECK(len == 0 || temp[len] == '\0');
    info->UpdateTranslatedSource(max_len ? temp.get() : NULL);
  }

  GLint status = GL_FALSE;
  glGetShaderiv(info->service_id(), GL_COMPILE_STATUS, &status);
  if (status) {
    info->SetStatus(true, "", translator);
    if (program_cache_) {
      const char* untranslated_source = source ? source->c_str() : "";
      program_cache_->ShaderCompilationSucceeded(untranslated_source);
    }
  } else {
    // We cannot reach here if we are using the shader translator.
    // All invalid shaders must be rejected by the translator.
    // All translated shaders must compile.
    GLint max_len = 0;
    glGetShaderiv(info->service_id(), GL_INFO_LOG_LENGTH, &max_len);
    scoped_array<char> temp(new char[max_len]);
    GLint len = 0;
    glGetShaderInfoLog(info->service_id(), max_len, &len, temp.get());
    DCHECK(max_len == 0 || len < max_len);
    DCHECK(len == 0 || temp[len] == '\0');
    info->SetStatus(false, std::string(temp.get(), len).c_str(), NULL);
    LOG_IF(ERROR, translator)
        << "Shader translator allowed/produced an invalid shader "
        << "unless the driver is buggy:"
        << "\n--original-shader--\n" << (source ? *source : "")
        << "\n--translated-shader--\n" << shader_src
        << "\n--info-log--\n" << *info->log_info();
  }
}

bool Program::Link(ShaderManager* manager,
                                       ShaderTranslator* vertex_translator,
                                       ShaderTranslator* fragment_translator,
                                       FeatureInfo* feature_info) {
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

  TimeTicks before_time = TimeTicks::HighResNow();
  bool link = true;
  ProgramCache* cache = manager_->program_cache_;
  if (cache) {
    ProgramCache::LinkedProgramStatus status = cache->GetLinkedProgramStatus(
        *attached_shaders_[0]->deferred_compilation_source(),
        *attached_shaders_[1]->deferred_compilation_source(),
        &bind_attrib_location_map_);

    if (status == ProgramCache::LINK_SUCCEEDED) {
      ProgramCache::ProgramLoadResult success = cache->LoadLinkedProgram(
                  service_id(),
                  attached_shaders_[0],
                  attached_shaders_[1],
                  &bind_attrib_location_map_);
      link = success != ProgramCache::PROGRAM_LOAD_SUCCESS;
      UMA_HISTOGRAM_BOOLEAN("GPU.ProgramCache.LoadBinarySuccess", !link);
    }

    if (link) {
      // compile our shaders if they're pending
      const int kShaders = Program::kMaxAttachedShaders;
      for (int i = 0; i < kShaders; ++i) {
        Shader* info = attached_shaders_[i].get();
        if (info->compilation_status() ==
            Shader::PENDING_DEFERRED_COMPILE) {
          ShaderTranslator* translator = ShaderIndexToTranslator(
              i,
              vertex_translator,
              fragment_translator);
          manager_->ForceCompileShader(info->deferred_compilation_source(),
                                       attached_shaders_[i],
                                       translator,
                                       feature_info);
          CHECK(info->IsValid());
        }
      }
    }
  }

  if (link) {
    before_time = TimeTicks::HighResNow();
    if (cache && gfx::g_driver_gl.ext.b_GL_ARB_get_program_binary) {
      glProgramParameteri(service_id(),
                          PROGRAM_BINARY_RETRIEVABLE_HINT,
                          GL_TRUE);
    }
    glLinkProgram(service_id());
  }

  GLint success = 0;
  glGetProgramiv(service_id(), GL_LINK_STATUS, &success);
  if (success == GL_TRUE) {
    Update();
    if (link) {
      if (cache) {
        cache->SaveLinkedProgram(service_id(),
                                 attached_shaders_[0],
                                 attached_shaders_[1],
                                 &bind_attrib_location_map_);
      }
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "GPU.ProgramCache.BinaryCacheMissTime",
          (TimeTicks::HighResNow() - before_time).InMicroseconds(),
          0,
          TimeDelta::FromSeconds(10).InMicroseconds(),
          50);
    } else {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "GPU.ProgramCache.BinaryCacheHitTime",
          (TimeTicks::HighResNow() - before_time).InMicroseconds(),
          0,
          TimeDelta::FromSeconds(1).InMicroseconds(),
          50);
    }
  } else {
    UpdateLogInfo();
  }
  return success == GL_TRUE;
}

void Program::Validate() {
  if (!IsValid()) {
    set_log_info("program not linked");
    return;
  }
  glValidateProgram(service_id());
  UpdateLogInfo();
}

GLint Program::GetUniformFakeLocation(
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
    if (!info.IsValid()) {
      continue;
    }
    if (info.name == name ||
        (info.is_array &&
         info.name.compare(0, info.name.size() - 3, name) == 0)) {
      return info.fake_location_base;
    } else if (getting_array_location && info.is_array) {
      // Look for an array specification.
      size_t open_pos_2 = info.name.find_last_of('[');
      if (open_pos_2 == open_pos &&
          name.compare(0, open_pos, info.name, 0, open_pos) == 0) {
        if (index >= 0 && index < info.size) {
          return ProgramManager::MakeFakeLocation(
              info.fake_location_base, index);
        }
      }
    }
  }
  return -1;
}

GLint Program::GetAttribLocation(
    const std::string& name) const {
  for (GLuint ii = 0; ii < attrib_infos_.size(); ++ii) {
    const VertexAttrib& info = attrib_infos_[ii];
    if (info.name == name) {
      return info.location;
    }
  }
  return -1;
}

const Program::UniformInfo*
    Program::GetUniformInfoByFakeLocation(
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
    if (!uniform_info.IsValid()) {
      return NULL;
    }
    GLint element_index = GetArrayElementIndexFromFakeLocation(fake_location);
    if (element_index < uniform_info.size) {
      *real_location = uniform_info.element_locations[element_index];
      *array_index = element_index;
      return &uniform_info;
    }
  }
  return NULL;
}

const std::string* Program::GetAttribMappedName(
    const std::string& original_name) const {
  for (int ii = 0; ii < kMaxAttachedShaders; ++ii) {
    Shader* shader_info = attached_shaders_[ii].get();
    if (shader_info) {
      const std::string* mapped_name =
          shader_info->GetAttribMappedName(original_name);
      if (mapped_name)
        return mapped_name;
    }
  }
  return NULL;
}

const std::string* Program::GetOriginalNameFromHashedName(
    const std::string& hashed_name) const {
  for (int ii = 0; ii < kMaxAttachedShaders; ++ii) {
    Shader* shader_info = attached_shaders_[ii].get();
    if (shader_info) {
      const std::string* original_name =
          shader_info->GetOriginalNameFromHashedName(hashed_name);
      if (original_name)
        return original_name;
    }
  }
  return NULL;
}

bool Program::SetUniformLocationBinding(
    const std::string& name, GLint location) {
  std::string short_name;
  int element_index = 0;
  if (!GetUniformNameSansElement(name, &element_index, &short_name) ||
      element_index != 0) {
    return false;
  }

  bind_uniform_location_map_[short_name] = location;
  return true;
}

// Note: This is only valid to call right after a program has been linked
// successfully.
void Program::GetCorrectedVariableInfo(
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
      Shader* shader_info = attached_shaders_[ii].get();
      if (shader_info) {
        const Shader::VariableInfo* variable_info =
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

bool Program::AddUniformInfo(
        GLsizei size, GLenum type, GLint location, GLint fake_base_location,
        const std::string& name, const std::string& original_name,
        size_t* next_available_index) {
  DCHECK(next_available_index);
  const char* kArraySpec = "[0]";
  size_t uniform_index =
      fake_base_location >= 0 ? fake_base_location : *next_available_index;
  if (uniform_infos_.size() < uniform_index + 1) {
    uniform_infos_.resize(uniform_index + 1);
  }

  // return if this location is already in use.
  if (uniform_infos_[uniform_index].IsValid()) {
    DCHECK_GE(fake_base_location, 0);
    return false;
  }

  uniform_infos_[uniform_index] = UniformInfo(
      size, type, uniform_index, original_name);
  ++num_uniforms_;

  UniformInfo& info = uniform_infos_[uniform_index];
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

  if (info.IsSampler()) {
    sampler_indices_.push_back(info.fake_location_base);
  }
  max_uniform_name_length_ =
      std::max(max_uniform_name_length_,
               static_cast<GLsizei>(info.name.size()));

  while (*next_available_index < uniform_infos_.size() &&
         uniform_infos_[*next_available_index].IsValid()) {
    *next_available_index = *next_available_index + 1;
  }

  return true;
}

const Program::UniformInfo*
    Program::GetUniformInfo(
        GLint index) const {
  if (static_cast<size_t>(index) >= uniform_infos_.size()) {
    return NULL;
  }

  const UniformInfo& info = uniform_infos_[index];
  return info.IsValid() ? &info : NULL;
}

bool Program::SetSamplers(
    GLint num_texture_units, GLint fake_location,
    GLsizei count, const GLint* value) {
  if (fake_location < 0) {
    return true;
  }
  GLint uniform_index = GetUniformInfoIndexFromFakeLocation(fake_location);
  if (uniform_index >= 0 &&
      static_cast<size_t>(uniform_index) < uniform_infos_.size()) {
    UniformInfo& info = uniform_infos_[uniform_index];
    if (!info.IsValid()) {
      return false;
    }
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

void Program::GetProgramiv(GLenum pname, GLint* params) {
  switch (pname) {
    case GL_ACTIVE_ATTRIBUTES:
      *params = attrib_infos_.size();
      break;
    case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:
      // Notice +1 to accomodate NULL terminator.
      *params = max_attrib_name_length_ + 1;
      break;
    case GL_ACTIVE_UNIFORMS:
      *params = num_uniforms_;
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

bool Program::AttachShader(
    ShaderManager* shader_manager,
    Shader* info) {
  DCHECK(shader_manager);
  DCHECK(info);
  int index = ShaderTypeToIndex(info->shader_type());
  if (attached_shaders_[index] != NULL) {
    return false;
  }
  attached_shaders_[index] = scoped_refptr<Shader>(info);
  shader_manager->UseShader(info);
  return true;
}

bool Program::DetachShader(
    ShaderManager* shader_manager,
    Shader* info) {
  DCHECK(shader_manager);
  DCHECK(info);
  if (attached_shaders_[ShaderTypeToIndex(info->shader_type())].get() != info) {
    return false;
  }
  attached_shaders_[ShaderTypeToIndex(info->shader_type())] = NULL;
  shader_manager->UnuseShader(info);
  return true;
}

void Program::DetachShaders(ShaderManager* shader_manager) {
  DCHECK(shader_manager);
  for (int ii = 0; ii < kMaxAttachedShaders; ++ii) {
    if (attached_shaders_[ii]) {
      DetachShader(shader_manager, attached_shaders_[ii]);
    }
  }
}

bool Program::CanLink() const {
  for (int ii = 0; ii < kMaxAttachedShaders; ++ii) {
    if (!attached_shaders_[ii] || !attached_shaders_[ii]->IsValid()) {
      return false;
    }
  }
  return true;
}

bool Program::DetectAttribLocationBindingConflicts() const {
  std::set<GLint> location_binding_used;
  for (LocationMap::const_iterator it = bind_attrib_location_map_.begin();
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

void Program::GetProgram(
    ProgramManager* manager, CommonDecoder::Bucket* bucket) const {
  // NOTE: It seems to me the math in here does not need check for overflow
  // because the data being calucated from has various small limits. The max
  // number of attribs + uniforms is somewhere well under 1024. The maximum size
  // of an identifier is 256 characters.
  uint32 num_locations = 0;
  uint32 total_string_size = 0;

  for (size_t ii = 0; ii < attrib_infos_.size(); ++ii) {
    const VertexAttrib& info = attrib_infos_[ii];
    num_locations += 1;
    total_string_size += info.name.size();
  }

  for (size_t ii = 0; ii < uniform_infos_.size(); ++ii) {
    const UniformInfo& info = uniform_infos_[ii];
    if (info.IsValid()) {
      num_locations += info.element_locations.size();
      total_string_size += info.name.size();
    }
  }

  uint32 num_inputs = attrib_infos_.size() + num_uniforms_;
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
  header->num_uniforms = num_uniforms_;

  for (size_t ii = 0; ii < attrib_infos_.size(); ++ii) {
    const VertexAttrib& info = attrib_infos_[ii];
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
    if (info.IsValid()) {
      inputs->size = info.size;
      inputs->type = info.type;
      inputs->location_offset = ComputeOffset(header, locations);
      inputs->name_offset = ComputeOffset(header, strings);
      inputs->name_length = info.name.size();
      DCHECK(static_cast<size_t>(info.size) == info.element_locations.size());
      for (size_t jj = 0; jj < info.element_locations.size(); ++jj) {
        *locations++ = ProgramManager::MakeFakeLocation(ii, jj);
      }
      memcpy(strings, info.name.c_str(), info.name.size());
      strings += info.name.size();
      ++inputs;
    }
  }

  DCHECK_EQ(ComputeOffset(header, strings), size);
}

Program::~Program() {
  if (manager_) {
    if (manager_->have_context_) {
      glDeleteProgram(service_id());
    }
    manager_->StopTracking(this);
    manager_ = NULL;
  }
}


ProgramManager::ProgramManager(ProgramCache* program_cache)
    : program_info_count_(0),
      have_context_(true),
      disable_workarounds_(
          CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kDisableGpuDriverBugWorkarounds)),
      program_cache_(program_cache) { }

ProgramManager::~ProgramManager() {
  DCHECK(program_infos_.empty());
}

void ProgramManager::Destroy(bool have_context) {
  have_context_ = have_context;
  program_infos_.clear();
}

void ProgramManager::StartTracking(Program* /* program */) {
  ++program_info_count_;
}

void ProgramManager::StopTracking(Program* /* program */) {
  --program_info_count_;
}

Program* ProgramManager::CreateProgram(
    GLuint client_id, GLuint service_id) {
  std::pair<ProgramInfoMap::iterator, bool> result =
      program_infos_.insert(
          std::make_pair(client_id,
                         scoped_refptr<Program>(
                             new Program(this, service_id))));
  DCHECK(result.second);
  return result.first->second;
}

Program* ProgramManager::GetProgram(GLuint client_id) {
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

ProgramCache* ProgramManager::program_cache() const {
  return program_cache_;
}

bool ProgramManager::IsOwned(Program* info) {
  for (ProgramInfoMap::iterator it = program_infos_.begin();
       it != program_infos_.end(); ++it) {
    if (it->second.get() == info) {
      return true;
    }
  }
  return false;
}

void ProgramManager::RemoveProgramInfoIfUnused(
    ShaderManager* shader_manager, Program* info) {
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
    Program* info) {
  DCHECK(shader_manager);
  DCHECK(info);
  DCHECK(IsOwned(info));
  info->MarkAsDeleted();
  RemoveProgramInfoIfUnused(shader_manager, info);
}

void ProgramManager::UseProgram(Program* info) {
  DCHECK(info);
  DCHECK(IsOwned(info));
  info->IncUseCount();
  ClearUniforms(info);
}

void ProgramManager::UnuseProgram(
    ShaderManager* shader_manager,
    Program* info) {
  DCHECK(shader_manager);
  DCHECK(info);
  DCHECK(IsOwned(info));
  info->DecUseCount();
  RemoveProgramInfoIfUnused(shader_manager, info);
}

void ProgramManager::ClearUniforms(Program* info) {
  DCHECK(info);
  if (!disable_workarounds_) {
    info->ClearUniforms(&zero_);
  }
}

int32 ProgramManager::MakeFakeLocation(int32 index, int32 element) {
  return index + element * 0x10000;
}

}  // namespace gles2
}  // namespace gpu
