// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/apps/js/bootstrap.h"

#include "base/message_loop/message_loop.h"
#include "gin/per_isolate_data.h"
#include "mojo/public/bindings/js/handle.h"

namespace mojo {
namespace apps {

namespace {

void Quit(const v8::FunctionCallbackInfo<v8::Value>& info) {
  base::MessageLoop::current()->QuitNow();
}

MojoHandle g_initial_handle = MOJO_HANDLE_INVALID;

gin::WrapperInfo g_wrapper_info = { gin::kEmbedderNativeGin };

}  // namespace

const char Bootstrap::kModuleName[] = "mojo/apps/js/bootstrap";

v8::Local<v8::ObjectTemplate> Bootstrap::GetTemplate(v8::Isolate* isolate) {
  gin::PerIsolateData* data = gin::PerIsolateData::From(isolate);
  v8::Local<v8::ObjectTemplate> templ = data->GetObjectTemplate(
      &g_wrapper_info);

  if (templ.IsEmpty()) {
    templ = v8::ObjectTemplate::New();
    templ->Set(gin::StringToSymbol(isolate, "quit"),
               v8::FunctionTemplate::New(Quit));

    // Don't forget to call SetInitialHandle before getting the template.
    DCHECK(g_initial_handle != MOJO_HANDLE_INVALID);
    templ->Set(gin::StringToSymbol(isolate, "initialHandle"),
               gin::ConvertToV8(isolate, g_initial_handle));

    data->SetObjectTemplate(&g_wrapper_info, templ);
  }

  return templ;
}

void Bootstrap::SetInitialHandle(MojoHandle pipe) {
  g_initial_handle = pipe;
}

}  // namespace apps
}  // namespace mojo
