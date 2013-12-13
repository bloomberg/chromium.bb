// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/apps/js/bindings/gl/opaque.h"

#include "gin/object_template_builder.h"
#include "gin/per_isolate_data.h"

namespace mojo {
namespace js {
namespace gl {

gin::WrapperInfo Opaque::kWrapperInfo = { gin::kEmbedderNativeGin };

gin::Handle<Opaque> Opaque::Create(v8::Isolate* isolate, GLuint value) {
  return gin::CreateHandle(isolate, new Opaque(value));
}

v8::Handle<v8::ObjectTemplate> Opaque::GetObjectTemplate(v8::Isolate* isolate) {
  gin::PerIsolateData* data = gin::PerIsolateData::From(isolate);
  v8::Local<v8::ObjectTemplate> templ = data->GetObjectTemplate(&kWrapperInfo);
  if (templ.IsEmpty()) {
    templ = gin::ObjectTemplateBuilder(isolate)
        .Build();
    templ->SetInternalFieldCount(gin::kNumberOfInternalFields);
    data->SetObjectTemplate(&kWrapperInfo, templ);
  }
  return templ;
}

Opaque::Opaque(GLuint value) : value_(value) {
}

}  // namespace gl
}  // namespace js
}  // namespace mojo
