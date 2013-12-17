// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/apps/js/bindings/gl/opaque.h"

#include "gin/object_template_builder.h"

namespace mojo {
namespace js {
namespace gl {

gin::WrapperInfo Opaque::kWrapperInfo = { gin::kEmbedderNativeGin };

gin::Handle<Opaque> Opaque::Create(v8::Isolate* isolate, GLuint value) {
  return gin::CreateHandle(isolate, new Opaque(value));
}

Opaque::Opaque(GLuint value) : value_(value) {
}

}  // namespace gl
}  // namespace js
}  // namespace mojo
