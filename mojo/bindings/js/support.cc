// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/bindings/js/support.h"

#include "base/bind.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/function_template.h"
#include "gin/object_template_builder.h"
#include "gin/per_isolate_data.h"
#include "gin/public/wrapper_info.h"
#include "gin/wrappable.h"
#include "mojo/bindings/js/handle.h"
#include "mojo/bindings/js/waiting_callback.h"
#include "mojo/public/cpp/environment/default_async_waiter.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {
namespace js {

namespace {

WaitingCallback* AsyncWait(const gin::Arguments& args, mojo::Handle handle,
                           MojoWaitFlags flags,
                           v8::Handle<v8::Function> callback) {
  gin::Handle<WaitingCallback> waiting_callback =
      WaitingCallback::Create(args.isolate(), callback);

  MojoAsyncWaiter* waiter = GetDefaultAsyncWaiter();
  MojoAsyncWaitID wait_id = waiter->AsyncWait(
      waiter,
      handle.value(),
      flags,
      MOJO_DEADLINE_INDEFINITE,
      &WaitingCallback::CallOnHandleReady,
      waiting_callback.get());

  waiting_callback->set_wait_id(wait_id);

  return waiting_callback.get();
}

void CancelWait(WaitingCallback* waiting_callback) {
  if (!waiting_callback->wait_id())
    return;

  MojoAsyncWaiter* waiter = GetDefaultAsyncWaiter();
  waiter->CancelWait(waiter, waiting_callback->wait_id());
  waiting_callback->set_wait_id(0);
}

gin::WrapperInfo g_wrapper_info = { gin::kEmbedderNativeGin };

}  // namespace

const char Support::kModuleName[] = "mojo/bindings/js/support";

v8::Local<v8::Value> Support::GetModule(v8::Isolate* isolate) {
  gin::PerIsolateData* data = gin::PerIsolateData::From(isolate);
  v8::Local<v8::ObjectTemplate> templ = data->GetObjectTemplate(
      &g_wrapper_info);

  if (templ.IsEmpty()) {
    templ = gin::ObjectTemplateBuilder(isolate)
                .SetMethod("asyncWait", AsyncWait)
                .SetMethod("cancelWait", CancelWait)
                .Build();

    data->SetObjectTemplate(&g_wrapper_info, templ);
  }

  return templ->NewInstance();
}

}  // namespace js
}  // namespace mojo
