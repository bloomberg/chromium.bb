// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shader_translator.h"

#include <string.h>

#include "base/at_exit.h"
#include "base/logging.h"

namespace {
void FinalizeShaderTranslator(void* /* dummy */) {
  ShFinalize();
}

bool InitializeShaderTranslator() {
  static bool initialized = false;
  if (!initialized && ShInitialize()) {
    base::AtExitManager::RegisterCallback(&FinalizeShaderTranslator, NULL);
    initialized = true;
  }
  return initialized;
}

using gpu::gles2::ShaderTranslator;
void GetVariableInfo(ShHandle compiler, ShShaderInfo var_type,
                     ShaderTranslator::VariableMap* var_map) {
  int name_len = 0;
  switch (var_type) {
    case SH_ACTIVE_ATTRIBUTES:
      ShGetInfo(compiler, SH_ACTIVE_ATTRIBUTE_MAX_LENGTH, &name_len);
      break;
    case SH_ACTIVE_UNIFORMS:
      ShGetInfo(compiler, SH_ACTIVE_UNIFORM_MAX_LENGTH, &name_len);
      break;
    default: NOTREACHED();
  }
  if (name_len <= 1) return;
  scoped_array<char> name(new char[name_len]);

  int num_vars = 0;
  ShGetInfo(compiler, var_type, &num_vars);
  for (int i = 0; i < num_vars; ++i) {
    int size = 0;
    ShDataType type = SH_NONE;

    switch (var_type) {
      case SH_ACTIVE_ATTRIBUTES:
        ShGetActiveAttrib(compiler, i, NULL, &size, &type, name.get(), NULL);
        break;
      case SH_ACTIVE_UNIFORMS:
        ShGetActiveUniform(compiler, i, NULL, &size, &type, name.get(), NULL);
        break;
      default: NOTREACHED();
    }

    ShaderTranslator::VariableInfo info(type, size);
    (*var_map)[name.get()] = info;
  }
}
}  // namespace

namespace gpu {
namespace gles2 {

ShaderTranslator::ShaderTranslator()
    : compiler_(NULL),
      implementation_is_glsl_es_(false) {
}

ShaderTranslator::~ShaderTranslator() {
  if (compiler_ != NULL)
    ShDestruct(compiler_);
}

bool ShaderTranslator::Init(ShShaderType shader_type,
                            ShShaderSpec shader_spec,
                            const ShBuiltInResources* resources,
                            bool implementation_is_glsl_es) {
  // Make sure Init is called only once.
  DCHECK(compiler_ == NULL);
  DCHECK(shader_type == SH_FRAGMENT_SHADER || shader_type == SH_VERTEX_SHADER);
  DCHECK(shader_spec == SH_GLES2_SPEC || shader_spec == SH_WEBGL_SPEC);
  DCHECK(resources != NULL);

  if (!InitializeShaderTranslator())
    return false;

  compiler_ = ShConstructCompiler(shader_type, shader_spec, resources);
  implementation_is_glsl_es_ = implementation_is_glsl_es;
  return compiler_ != NULL;
}

bool ShaderTranslator::Translate(const char* shader) {
  // Make sure this instance is initialized.
  DCHECK(compiler_ != NULL);
  DCHECK(shader != NULL);
  ClearResults();

  bool success = false;
  int compile_options = SH_OBJECT_CODE | SH_ATTRIBUTES_UNIFORMS;
  if (ShCompile(compiler_, &shader, 1, compile_options)) {
    success = true;
    if (!implementation_is_glsl_es_) {
      // Get translated shader.
      int obj_code_len = 0;
      ShGetInfo(compiler_, SH_OBJECT_CODE_LENGTH, &obj_code_len);
      if (obj_code_len > 1) {
        translated_shader_.reset(new char[obj_code_len]);
        ShGetObjectCode(compiler_, translated_shader_.get());
      }
    } else {
      // Pass down the original shader's source rather than the
      // compiler's output. TODO(kbr): once the shader compiler has a
      // GLSL ES backend, use its output.
      int shader_code_len = 1 + strlen(shader);
      if (shader_code_len > 1) {
        translated_shader_.reset(new char[shader_code_len]);
        strncpy(translated_shader_.get(), shader, shader_code_len);
      }
    }
    // Get info for attribs and uniforms.
    GetVariableInfo(compiler_, SH_ACTIVE_ATTRIBUTES, &attrib_map_);
    GetVariableInfo(compiler_, SH_ACTIVE_UNIFORMS, &uniform_map_);
  }

  // Get info log.
  int info_log_len = 0;
  ShGetInfo(compiler_, SH_INFO_LOG_LENGTH, &info_log_len);
  if (info_log_len > 1) {
    info_log_.reset(new char[info_log_len]);
    ShGetInfoLog(compiler_, info_log_.get());
  } else {
    info_log_.reset();
  }

  return success;
}

const char* ShaderTranslator::translated_shader() const {
  return translated_shader_.get();
}

const char* ShaderTranslator::info_log() const {
  return info_log_.get();
}

const ShaderTranslatorInterface::VariableMap&
ShaderTranslator::attrib_map() const {
  return attrib_map_;
}

const ShaderTranslatorInterface::VariableMap&
ShaderTranslator::uniform_map() const {
  return uniform_map_;
}

void ShaderTranslator::ClearResults() {
  translated_shader_.reset();
  info_log_.reset();
  attrib_map_.clear();
  uniform_map_.clear();
}

}  // namespace gles2
}  // namespace gpu

