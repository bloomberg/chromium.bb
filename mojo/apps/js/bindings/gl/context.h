// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_APPS_JS_BINDINGS_GL_CONTEXT_H_
#define MOJO_APPS_JS_BINDINGS_GL_CONTEXT_H_

#include <GLES2/gl2.h>

#include "gin/handle.h"
#include "gin/public/wrapper_info.h"
#include "gin/wrappable.h"
#include "v8/include/v8.h"

namespace gin {
class Arguments;
class ArrayBufferView;
}

namespace mojo {
namespace js {
namespace gl {

// Context implements WebGLRenderingContext.
class Context : public gin::Wrappable<Context> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static gin::Handle<Context> Create(v8::Isolate* isolate, uint64_t encoded,
                                     int width, int height);

  static void BufferData(GLenum target, const gin::ArrayBufferView& buffer,
                         GLenum usage);
  static void CompileShader(const gin::Arguments& args, GLuint shader);
  static GLuint CreateBuffer();
  static void DrawElements(GLenum mode, GLsizei count, GLenum type,
                           uint64_t indices);
  static GLint GetAttribLocation(GLuint program, const std::string& name);
  static std::string GetProgramInfoLog(GLuint program);
  static std::string GetShaderInfoLog(GLuint shader);
  static GLint GetUniformLocation(GLuint program, const std::string& name);
  static void ShaderSource(GLuint shader, const std::string& source);
  static void UniformMatrix4fv(GLint location, GLboolean transpose,
                               const gin::ArrayBufferView& buffer);
  static void VertexAttribPointer(GLuint index, GLint size, GLenum type,
                                  GLboolean normalized, GLsizei stride,
                                  uint64_t offset);

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
