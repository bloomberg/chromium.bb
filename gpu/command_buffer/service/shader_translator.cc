// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shader_translator.h"

#include <string.h>
#include <GLES2/gl2.h>
#include <algorithm>

#include "base/at_exit.h"
#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"

namespace {

using gpu::gles2::ShaderTranslator;

class ShaderTranslatorInitializer {
 public:
  ShaderTranslatorInitializer() {
    TRACE_EVENT0("gpu", "ShInitialize");
    CHECK(ShInitialize());
  }

  ~ShaderTranslatorInitializer() {
    TRACE_EVENT0("gpu", "ShFinalize");
    ShFinalize();
  }
};

base::LazyInstance<ShaderTranslatorInitializer> g_translator_initializer =
    LAZY_INSTANCE_INITIALIZER;

void GetVariableInfo(ShHandle compiler, ShShaderInfo var_type,
                     ShaderTranslator::VariableMap* var_map) {
  if (!var_map)
    return;
  var_map->clear();

  size_t name_len = 0, mapped_name_len = 0;
  switch (var_type) {
    case SH_ACTIVE_ATTRIBUTES:
      ShGetInfo(compiler, SH_ACTIVE_ATTRIBUTE_MAX_LENGTH, &name_len);
      break;
    case SH_ACTIVE_UNIFORMS:
      ShGetInfo(compiler, SH_ACTIVE_UNIFORM_MAX_LENGTH, &name_len);
      break;
    case SH_VARYINGS:
      ShGetInfo(compiler, SH_VARYING_MAX_LENGTH, &name_len);
      break;
    default: NOTREACHED();
  }
  ShGetInfo(compiler, SH_MAPPED_NAME_MAX_LENGTH, &mapped_name_len);
  if (name_len <= 1 || mapped_name_len <= 1) return;
  scoped_ptr<char[]> name(new char[name_len]);
  scoped_ptr<char[]> mapped_name(new char[mapped_name_len]);

  size_t num_vars = 0;
  ShGetInfo(compiler, var_type, &num_vars);
  for (size_t i = 0; i < num_vars; ++i) {
    size_t len = 0;
    int size = 0;
    sh::GLenum type = GL_NONE;
    ShPrecisionType precision = SH_PRECISION_UNDEFINED;
    int static_use = 0;

    ShGetVariableInfo(compiler, var_type, i,
                      &len, &size, &type, &precision, &static_use,
                      name.get(), mapped_name.get());

    // In theory we should CHECK(len <= name_len - 1) here, but ANGLE needs
    // to handle long struct field name mapping before we can do this.
    // Also, we should modify the ANGLE interface to also return a length
    // for mapped_name.
    std::string name_string(name.get(), std::min(len, name_len - 1));
    mapped_name.get()[mapped_name_len - 1] = '\0';

    ShaderTranslator::VariableInfo info(
        type, size, precision, static_use, name_string);
    (*var_map)[mapped_name.get()] = info;
  }
}

void GetNameHashingInfo(
    ShHandle compiler, ShaderTranslator::NameMap* name_map) {
  if (!name_map)
    return;
  name_map->clear();

  size_t hashed_names_count = 0;
  ShGetInfo(compiler, SH_HASHED_NAMES_COUNT, &hashed_names_count);
  if (hashed_names_count == 0)
    return;

  size_t name_max_len = 0, hashed_name_max_len = 0;
  ShGetInfo(compiler, SH_NAME_MAX_LENGTH, &name_max_len);
  ShGetInfo(compiler, SH_HASHED_NAME_MAX_LENGTH, &hashed_name_max_len);

  scoped_ptr<char[]> name(new char[name_max_len]);
  scoped_ptr<char[]> hashed_name(new char[hashed_name_max_len]);

  for (size_t i = 0; i < hashed_names_count; ++i) {
    ShGetNameHashingEntry(compiler, i, name.get(), hashed_name.get());
    (*name_map)[hashed_name.get()] = name.get();
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
      driver_bug_workarounds_(static_cast<ShCompileOptions>(0)) {
}

bool ShaderTranslator::Init(
    GLenum shader_type,
    ShShaderSpec shader_spec,
    const ShBuiltInResources* resources,
    ShaderTranslatorInterface::GlslImplementationType glsl_implementation_type,
    ShCompileOptions driver_bug_workarounds) {
  // Make sure Init is called only once.
  DCHECK(compiler_ == NULL);
  DCHECK(shader_type == GL_FRAGMENT_SHADER || shader_type == GL_VERTEX_SHADER);
  DCHECK(shader_spec == SH_GLES2_SPEC || shader_spec == SH_WEBGL_SPEC);
  DCHECK(resources != NULL);

  g_translator_initializer.Get();

  ShShaderOutput shader_output =
      (glsl_implementation_type == kGlslES ? SH_ESSL_OUTPUT : SH_GLSL_OUTPUT);

  {
    TRACE_EVENT0("gpu", "ShConstructCompiler");
    compiler_ = ShConstructCompiler(
        shader_type, shader_spec, shader_output, resources);
  }
  compiler_options_ = *resources;
  implementation_is_glsl_es_ = (glsl_implementation_type == kGlslES);
  driver_bug_workarounds_ = driver_bug_workarounds;
  return compiler_ != NULL;
}

int ShaderTranslator::GetCompileOptions() const {
  int compile_options =
      SH_OBJECT_CODE | SH_VARIABLES | SH_ENFORCE_PACKING_RESTRICTIONS |
      SH_LIMIT_EXPRESSION_COMPLEXITY | SH_LIMIT_CALL_STACK_DEPTH |
      SH_CLAMP_INDIRECT_ARRAY_BOUNDS;

  compile_options |= driver_bug_workarounds_;

  return compile_options;
}

bool ShaderTranslator::Translate(const std::string& shader_source,
                                 std::string* info_log,
                                 std::string* translated_source,
                                 VariableMap* attrib_map,
                                 VariableMap* uniform_map,
                                 VariableMap* varying_map,
                                 NameMap* name_map) const {
  // Make sure this instance is initialized.
  DCHECK(compiler_ != NULL);

  bool success = false;
  {
    TRACE_EVENT0("gpu", "ShCompile");
    const char* const shader_strings[] = { shader_source.c_str() };
    success = !!ShCompile(
        compiler_, shader_strings, 1, GetCompileOptions());
  }
  if (success) {
    if (translated_source) {
      translated_source->clear();
      // Get translated shader.
      size_t obj_code_len = 0;
      ShGetInfo(compiler_, SH_OBJECT_CODE_LENGTH, &obj_code_len);
      if (obj_code_len > 1) {
        scoped_ptr<char[]> buffer(new char[obj_code_len]);
        ShGetObjectCode(compiler_, buffer.get());
        *translated_source = std::string(buffer.get(), obj_code_len - 1);
      }
    }
    // Get info for attribs, uniforms, and varyings.
    GetVariableInfo(compiler_, SH_ACTIVE_ATTRIBUTES, attrib_map);
    GetVariableInfo(compiler_, SH_ACTIVE_UNIFORMS, uniform_map);
    GetVariableInfo(compiler_, SH_VARYINGS, varying_map);
    // Get info for name hashing.
    GetNameHashingInfo(compiler_, name_map);
  }

  // Get info log.
  if (info_log) {
    info_log->clear();
    size_t info_log_len = 0;
    ShGetInfo(compiler_, SH_INFO_LOG_LENGTH, &info_log_len);
    if (info_log_len > 1) {
      scoped_ptr<char[]> buffer(new char[info_log_len]);
      ShGetInfoLog(compiler_, buffer.get());
      *info_log = std::string(buffer.get(), info_log_len - 1);
    }
  }

  return success;
}

std::string ShaderTranslator::GetStringForOptionsThatWouldAffectCompilation()
    const {
  DCHECK(compiler_ != NULL);

  size_t resource_len = 0;
  ShGetInfo(compiler_, SH_RESOURCES_STRING_LENGTH, &resource_len);
  DCHECK(resource_len > 1);
  scoped_ptr<char[]> resource_str(new char[resource_len]);

  ShGetBuiltInResourcesString(compiler_, resource_len, resource_str.get());

  return std::string(":CompileOptions:" +
         base::IntToString(GetCompileOptions())) +
         std::string(resource_str.get());
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

}  // namespace gles2
}  // namespace gpu

