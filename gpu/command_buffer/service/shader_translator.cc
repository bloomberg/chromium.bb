// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shader_translator.h"

#include <string.h>
#include <algorithm>

#include "base/at_exit.h"
#include "base/logging.h"

namespace {

using gpu::gles2::ShaderTranslator;

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

void GetVariableInfo(ShHandle compiler, ShShaderInfo var_type,
                     ShaderTranslator::VariableMap* var_map) {
  int name_len = 0, mapped_name_len = 0;
  switch (var_type) {
    case SH_ACTIVE_ATTRIBUTES:
      ShGetInfo(compiler, SH_ACTIVE_ATTRIBUTE_MAX_LENGTH, &name_len);
      break;
    case SH_ACTIVE_UNIFORMS:
      ShGetInfo(compiler, SH_ACTIVE_UNIFORM_MAX_LENGTH, &name_len);
      break;
    default: NOTREACHED();
  }
  ShGetInfo(compiler, SH_MAPPED_NAME_MAX_LENGTH, &mapped_name_len);
  if (name_len <= 1 || mapped_name_len <= 1) return;
  scoped_array<char> name(new char[name_len]);
  scoped_array<char> mapped_name(new char[mapped_name_len]);

  int num_vars = 0;
  ShGetInfo(compiler, var_type, &num_vars);
  for (int i = 0; i < num_vars; ++i) {
    int len = 0;
    int size = 0;
    ShDataType type = SH_NONE;

    switch (var_type) {
      case SH_ACTIVE_ATTRIBUTES:
        ShGetActiveAttrib(
            compiler, i, &len, &size, &type, name.get(), mapped_name.get());
        break;
      case SH_ACTIVE_UNIFORMS:
        ShGetActiveUniform(
            compiler, i, &len, &size, &type, name.get(), mapped_name.get());
        break;
      default: NOTREACHED();
    }

    // In theory we should CHECK(len <= name_len - 1) here, but ANGLE needs
    // to handle long struct field name mapping before we can do this.
    // Also, we should modify the ANGLE interface to also return a length
    // for mapped_name.
    std::string name_string(name.get(), std::min(len, name_len - 1));
    mapped_name.get()[mapped_name_len - 1] = '\0';

    ShaderTranslator::VariableInfo info(type, size, name_string);
    (*var_map)[mapped_name.get()] = info;
  }
}

}  // namespace

namespace gpu {
namespace gles2 {

ShaderTranslator::DestructionObserver::DestructionObserver() {
}

ShaderTranslator::DestructionObserver::~DestructionObserver() {
}

ShaderTranslator::ShaderTranslator()
    : compiler_(NULL),
      implementation_is_glsl_es_(false),
      needs_built_in_function_emulation_(false) {
}

bool ShaderTranslator::Init(
    ShShaderType shader_type,
    ShShaderSpec shader_spec,
    const ShBuiltInResources* resources,
    ShaderTranslatorInterface::GlslImplementationType glsl_implementation_type,
    ShaderTranslatorInterface::GlslBuiltInFunctionBehavior
        glsl_built_in_function_behavior) {
  // Make sure Init is called only once.
  DCHECK(compiler_ == NULL);
  DCHECK(shader_type == SH_FRAGMENT_SHADER || shader_type == SH_VERTEX_SHADER);
  DCHECK(shader_spec == SH_GLES2_SPEC || shader_spec == SH_WEBGL_SPEC);
  DCHECK(resources != NULL);

  if (!InitializeShaderTranslator())
    return false;

  ShShaderOutput shader_output =
      (glsl_implementation_type == kGlslES ? SH_ESSL_OUTPUT : SH_GLSL_OUTPUT);

  compiler_ = ShConstructCompiler(
      shader_type, shader_spec, shader_output, resources);
  implementation_is_glsl_es_ = (glsl_implementation_type == kGlslES);
  needs_built_in_function_emulation_ =
      (glsl_built_in_function_behavior == kGlslBuiltInFunctionEmulated);
  return compiler_ != NULL;
}

bool ShaderTranslator::Translate(const char* shader) {
  // Make sure this instance is initialized.
  DCHECK(compiler_ != NULL);
  DCHECK(shader != NULL);
  ClearResults();

  bool success = false;
  int compile_options =
      SH_OBJECT_CODE | SH_ATTRIBUTES_UNIFORMS |
      SH_MAP_LONG_VARIABLE_NAMES | SH_CLAMP_INDIRECT_ARRAY_BOUNDS;
  if (needs_built_in_function_emulation_)
    compile_options |= SH_EMULATE_BUILT_IN_FUNCTIONS;
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

void ShaderTranslator::AddDestructionObserver(
    DestructionObserver* observer) {
  destruction_observers_.AddObserver(observer);
}

void ShaderTranslator::RemoveDestructionObserver(
    DestructionObserver* observer) {
  destruction_observers_.RemoveObserver(observer);
}

ShaderTranslator::~ShaderTranslator() {
  FOR_EACH_OBSERVER(DestructionObserver,
                    destruction_observers_,
                    OnDestruct(this));

  if (compiler_ != NULL)
    ShDestruct(compiler_);
}

void ShaderTranslator::ClearResults() {
  translated_shader_.reset();
  info_log_.reset();
  attrib_map_.clear();
  uniform_map_.clear();
}

}  // namespace gles2
}  // namespace gpu

