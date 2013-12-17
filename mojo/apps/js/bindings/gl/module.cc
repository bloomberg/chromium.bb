// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/apps/js/bindings/gl/module.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "base/logging.h"
#include "gin/arguments.h"
#include "gin/object_template_builder.h"
#include "gin/per_isolate_data.h"
#include "gin/wrappable.h"
#include "mojo/apps/js/bindings/gl/context.h"

namespace mojo {
namespace js {
namespace gl {

const char* kModuleName = "mojo/apps/js/bindings/gl";

namespace {

gin::WrapperInfo kWrapperInfo = { gin::kEmbedderNativeGin };

gin::Handle<Context> CreateContext(const gin::Arguments& args, uint64_t encoded,
                                   int width, int height) {
  return Context::Create(args.isolate(), encoded, width, height);
}

}  // namespace

v8::Local<v8::ObjectTemplate> GetModuleTemplate(v8::Isolate* isolate) {
  gin::PerIsolateData* data = gin::PerIsolateData::From(isolate);
  v8::Local<v8::ObjectTemplate> templ = data->GetObjectTemplate(&kWrapperInfo);

  if (templ.IsEmpty()) {
    templ = gin::ObjectTemplateBuilder(isolate)
        .SetMethod("Context", CreateContext)
        .Build();
    data->SetObjectTemplate(&kWrapperInfo, templ);
  }

  return templ;
}

}  // namespace gl
}  // namespace js
}  // namespace mojo
