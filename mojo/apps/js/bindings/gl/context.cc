// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/apps/js/bindings/gl/context.h"

#include <GLES2/gl2.h>

#include "gin/arguments.h"
#include "gin/object_template_builder.h"
#include "gin/per_isolate_data.h"
#include "mojo/public/gles2/gles2.h"

namespace mojo {
namespace js {
namespace gl {

gin::WrapperInfo Context::kWrapperInfo = { gin::kEmbedderNativeGin };

gin::Handle<Context> Context::Create(v8::Isolate* isolate, uint64_t encoded,
                                     int width, int height) {
  return gin::CreateHandle(isolate, new Context(encoded, width, height));
}

v8::Handle<v8::ObjectTemplate> Context::GetObjectTemplate(
    v8::Isolate* isolate) {
  gin::PerIsolateData* data = gin::PerIsolateData::From(isolate);
  v8::Local<v8::ObjectTemplate> templ = data->GetObjectTemplate(&kWrapperInfo);
  if (templ.IsEmpty()) {
    templ = gin::ObjectTemplateBuilder(isolate)
        .SetValue("VERTEX_SHADER", GL_VERTEX_SHADER)
        .SetMethod("createShader", CreateShader)
        .SetMethod("shaderSource", ShaderSource)
        .SetMethod("compileShader", CompileShader)
        .Build();
    templ->SetInternalFieldCount(gin::kNumberOfInternalFields);
    data->SetObjectTemplate(&kWrapperInfo, templ);
  }
  return templ;
}

gin::Handle<Shader> Context::CreateShader(const gin::Arguments& args,
                                          GLenum type) {
  gin::Handle<Shader> result;
  GLuint glshader = glCreateShader(type);
  if (glshader != 0u) {
    result = Opaque::Create(args.isolate(), glshader);
  }
  return result;
}

void Context::ShaderSource(gin::Handle<Shader> shader,
                           const std::string& source) {
  const char* source_chars = source.c_str();
  glShaderSource(shader->value(), 1, &source_chars, NULL);
}

void Context::CompileShader(const gin::Arguments& args,
                            gin::Handle<Shader> shader) {
  glCompileShader(shader->value());
  GLint compiled = 0;
  glGetShaderiv(shader->value(), GL_COMPILE_STATUS, &compiled);
  if (!compiled) {
    // Or should |shader| do it when it is destroyed?
    glDeleteShader(shader->value());
    args.ThrowTypeError("Could not compile shader");
    return;
  }
}

Context::Context(uint64_t encoded, int width, int height)
    : encoded_(encoded) {
  // TODO(aa): When we want to support multiple contexts, we should add
  // Context::MakeCurrent() for developers to switch between them.
  MojoGLES2MakeCurrent(encoded_);
}

}  // namespace gl
}  // namespace js
}  // namespace mojo
