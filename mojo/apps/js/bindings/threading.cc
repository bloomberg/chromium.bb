// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/apps/js/bindings/threading.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "gin/function_template.h"
#include "gin/object_template_builder.h"
#include "gin/per_isolate_data.h"
#include "mojo/apps/js/bindings/handle.h"

namespace mojo {
namespace apps {

namespace {

void Quit() {
  base::MessageLoop::current()->QuitNow();
}

gin::WrapperInfo g_wrapper_info = { gin::kEmbedderNativeGin };

}  // namespace

const char Threading::kModuleName[] = "mojo/apps/js/bindings/threading";

v8::Local<v8::ObjectTemplate> Threading::GetTemplate(v8::Isolate* isolate) {
  gin::PerIsolateData* data = gin::PerIsolateData::From(isolate);
  v8::Local<v8::ObjectTemplate> templ = data->GetObjectTemplate(
      &g_wrapper_info);

  if (templ.IsEmpty()) {
    templ = gin::ObjectTemplateBuilder(isolate)
        .SetMethod("quit", Quit)
        .Build();

    data->SetObjectTemplate(&g_wrapper_info, templ);
  }

  return templ;
}

}  // namespace apps
}  // namespace mojo
