// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/apps/js/bindings/support.h"

#include "base/bind.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/function_template.h"
#include "gin/object_template_builder.h"
#include "gin/per_isolate_data.h"
#include "gin/public/wrapper_info.h"
#include "gin/wrappable.h"
#include "mojo/apps/js/bindings/handle.h"
#include "mojo/apps/js/bindings/waiting_callback.h"
#include "mojo/public/bindings/lib/bindings_support.h"

namespace mojo {
namespace js {

namespace {

WaitingCallback* AsyncWait(const gin::Arguments& args, mojo::Handle handle,
                           MojoWaitFlags flags,
                           v8::Handle<v8::Function> callback) {
  scoped_refptr<WaitingCallback> waiting_callback =
      WaitingCallback::Create(args.isolate(), callback);

  BindingsSupport::AsyncWaitID wait_id = BindingsSupport::Get()->AsyncWait(
      handle, flags, waiting_callback.get());

  waiting_callback->set_wait_id(wait_id);

  return waiting_callback.get();
}

void CancelWait(WaitingCallback* waiting_callback) {
  if (!waiting_callback->wait_id())
    return;
  BindingsSupport::Get()->CancelWait(waiting_callback->wait_id());
  waiting_callback->set_wait_id(NULL);
}

gin::WrapperInfo g_wrapper_info = { gin::kEmbedderNativeGin };

}  // namespace

const char Support::kModuleName[] = "mojo/apps/js/bindings/support";

v8::Local<v8::ObjectTemplate> Support::GetTemplate(v8::Isolate* isolate) {
  gin::PerIsolateData* data = gin::PerIsolateData::From(isolate);
  v8::Local<v8::ObjectTemplate> templ = data->GetObjectTemplate(
      &g_wrapper_info);

  if (templ.IsEmpty()) {
    WaitingCallback::EnsureRegistered(isolate);

     templ = gin::ObjectTemplateBuilder(isolate)
        .SetMethod("asyncWait", AsyncWait)
        .SetMethod("cancelWait", CancelWait)
        .Build();

    data->SetObjectTemplate(&g_wrapper_info, templ);
  }

  return templ;
}

}  // namespace js
}  // namespace mojo
