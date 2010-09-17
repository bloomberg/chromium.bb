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
  int compile_options = EShOptObjectCode;
  if (ShCompile(compiler_, &shader, 1, compile_options)) {
    // Get translated shader.
    int obj_code_len = 0;
    ShGetInfo(compiler_, SH_OBJECT_CODE_LENGTH, &obj_code_len);
    translated_shader_.reset(new char[obj_code_len]);
    ShGetObjectCode(compiler_, translated_shader_.get());

    // TODO(alokp): Get attribs and uniforms.
    success = true;
  }

  // Get info log.
  int info_log_len = 0;
  ShGetInfo(compiler_, SH_INFO_LOG_LENGTH, &info_log_len);
  info_log_.reset(new char[info_log_len]);
  ShGetInfoLog(compiler_, info_log_.get());

  return success;
}

void ShaderTranslator::ClearResults() {
  translated_shader_.reset();
  info_log_.reset();
}

}  // namespace gles2
}  // namespace gpu

