// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_APPS_JS_BINDINGS_GL_CONTEXT_H_
#define MOJO_APPS_JS_BINDINGS_GL_CONTEXT_H_

#include <GLES2/gl2.h>

#include "gin/handle.h"
#include "gin/public/wrapper_info.h"
#include "gin/wrappable.h"
#include "mojo/apps/js/bindings/gl/opaque.h"
#include "v8/include/v8.h"

namespace gin {
class Arguments;
}

namespace mojo {
namespace js {
namespace gl {

typedef Opaque Shader;

// Context implements WebGLRenderingContext.
class Context : public gin::Wrappable<Context> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static gin::Handle<Context> Create(v8::Isolate* isolate, uint64_t encoded,
                                     int width, int height);
  static gin::Handle<Shader> CreateShader(const gin::Arguments& arguments,
                                          GLenum type);
  static void ShaderSource(gin::Handle<Shader> shader,
                           const std::string& source);
  static void CompileShader(const gin::Arguments& arguments,
                            gin::Handle<Shader> shader);

 private:
  virtual gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) OVERRIDE;

  Context(uint64_t encoded, int width, int height);

  uint64_t encoded_;
};

}  // namespace gl
}  // namespace js
}  // namespace mojo

#endif  // MOJO_APPS_JS_BINDINGS_GL_CONTEXT_H_
