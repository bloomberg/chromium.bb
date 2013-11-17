// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/js/global.h"

#include "gin/modules/module_registry.h"
#include "gin/per_isolate_data.h"
#include "gin/wrapper_info.h"

namespace mojo {
namespace js {

namespace {

gin::WrapperInfo g_wrapper_info = {};

}

v8::Local<v8::ObjectTemplate> GetGlobalTemplate(v8::Isolate* isolate) {
  gin::PerIsolateData* data = gin::PerIsolateData::From(isolate);
  v8::Local<v8::ObjectTemplate> templ = data->GetObjectTemplate(
      &g_wrapper_info);
  if (templ.IsEmpty()) {
    templ = v8::ObjectTemplate::New();
    gin::ModuleRegistry::RegisterGlobals(isolate, templ);
    data->SetObjectTemplate(&g_wrapper_info, templ);
  }
  return templ;
}

}  // namespace js
}  // namespace mojo
