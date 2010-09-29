// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shader_translator.h"

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
void GetVariableInfo(ShHandle compiler, EShInfo var_type,
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
    EShDataType type = SH_NONE;

    switch (var_type) {
      case SH_ACTIVE_ATTRIBUTES:
        ShGetActiveAttrib(compiler, i, NULL, &size, &type, name.get());
        break;
      case SH_ACTIVE_UNIFORMS:
        ShGetActiveUniform(compiler, i, NULL, &size, &type, name.get());
        break;
      default: NOTREACHED();
    }

    ShaderTranslator::VariableInfo info = {0};
    info.type = type;
    info.size = size;
    (*var_map)[name.get()] = info;
  }
}
}  // namespace

namespace gpu {
namespace gles2 {

ShaderTranslator::ShaderTranslator() : compiler_(NULL) {
}

ShaderTranslator::~ShaderTranslator() {
  if (compiler_ != NULL)
    ShDestruct(compiler_);
}

bool ShaderTranslator::Init(EShLanguage language,
                            const TBuiltInResource* resources) {
  // Make sure Init is called only once.
  DCHECK(compiler_ == NULL);
  DCHECK(language == EShLangVertex || language == EShLangFragment);
  DCHECK(resources != NULL);

  if (!InitializeShaderTranslator())
    return false;

  compiler_ = ShConstructCompiler(language, EShSpecGLES2, resources);
  return compiler_ != NULL;
}

bool ShaderTranslator::Translate(const char* shader) {
  // Make sure this instance is initialized.
  DCHECK(compiler_ != NULL);
  DCHECK(shader != NULL);
  ClearResults();

  bool success = false;
  int compile_options = EShOptObjectCode | EShOptAttribsUniforms;
  if (ShCompile(compiler_, &shader, 1, compile_options)) {
    success = true;
    // Get translated shader.
    int obj_code_len = 0;
    ShGetInfo(compiler_, SH_OBJECT_CODE_LENGTH, &obj_code_len);
    if (obj_code_len > 1) {
      translated_shader_.reset(new char[obj_code_len]);
      ShGetObjectCode(compiler_, translated_shader_.get());
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
  }

  return success;
}

void ShaderTranslator::ClearResults() {
  translated_shader_.reset();
  info_log_.reset();
  attrib_map_.clear();
  uniform_map_.clear();
}

}  // namespace gles2
}  // namespace gpu

