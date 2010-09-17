// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHADER_TRANSLATOR_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHADER_TRANSLATOR_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "third_party/angle/include/GLSLANG/ShaderLang.h"

namespace gpu {
namespace gles2 {

// Translates GLSL ES 2.0 shader to desktop GLSL shader.
class ShaderTranslator {
 public:
  ShaderTranslator();
  ~ShaderTranslator();

  // Initializes the translator.
  // Must be called once before using the translator object.
  bool Init(EShLanguage language, const TBuiltInResource* resources);
  // Translates the given shader source.
  // Returns true if translation is successful, false otherwise.
  bool Translate(const char* shader);

  // The following functions return results from the last translation.
  // The results are NULL/empty if the translation was unsuccessful.
  // A valid info-log is always returned irrespective of whether translation
  // was successful or not.
  const char* translated_shader() { return translated_shader_.get(); }
  const char* info_log() { return info_log_.get(); }

  // TODO(alokp): Add functions for returning attribs and uniforms.

 private:
  void ClearResults();
  DISALLOW_COPY_AND_ASSIGN(ShaderTranslator);

  ShHandle compiler_;
  scoped_array<char> translated_shader_;
  scoped_array<char> info_log_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHADER_TRANSLATOR_H_

