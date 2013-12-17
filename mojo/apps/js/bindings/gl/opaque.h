// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_APPS_JS_BINDINGS_GL_OPAQUE_H_
#define MOJO_APPS_JS_BINDINGS_GL_OPAQUE_H_

#include <GLES2/gl2.h>

#include "gin/handle.h"
#include "gin/public/wrapper_info.h"
#include "gin/wrappable.h"
#include "v8/include/v8.h"

namespace mojo {
namespace js {
namespace gl {

// WebGL has many interfaces, such as WebGLObject, WebGLBuffer, etc, which wrap
// integers that are opaque to script. This class is used for all those objects.
class Opaque : public gin::Wrappable<Opaque> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static gin::Handle<Opaque> Create(v8::Isolate* isolate, GLuint value);

  GLuint value() const { return value_; }
  void set_value(GLuint val) { value_ = val; }

 private:
  Opaque(GLuint value);

  GLuint value_;
};

}  // namespace gl
}  // namespace js
}  // namespace mojo

#endif  // MOJO_APPS_JS_BINDINGS_GL_CONTEXT_H_
