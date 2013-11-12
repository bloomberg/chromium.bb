// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/js/mojo.h"

#include "gin/converter.h"
#include "gin/per_isolate_data.h"
#include "gin/wrapper_info.h"
#include "mojo/public/bindings/js/core.h"

namespace mojo {
namespace js {

namespace {

gin::WrapperInfo g_mojo_wrapper_info = {};

}

v8::Local<v8::ObjectTemplate> GetMojoTemplate(v8::Isolate* isolate) {
  gin::PerIsolateData* data = gin::PerIsolateData::From(isolate);
  v8::Local<v8::ObjectTemplate> templ = data->GetObjectTemplate(
      &g_mojo_wrapper_info);
  if (templ.IsEmpty()) {
    templ = v8::ObjectTemplate::New();
    templ->Set(gin::StringToSymbol(isolate, "core"), GetCoreTemplate(isolate));
    data->SetObjectTemplate(&g_mojo_wrapper_info, templ);
  }
  return templ;
}

}  // namespace js
}  // namespace mojo
