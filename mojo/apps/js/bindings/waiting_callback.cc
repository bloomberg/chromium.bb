// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/apps/js/bindings/waiting_callback.h"

#include "gin/per_context_data.h"
#include "gin/per_isolate_data.h"

INIT_WRAPPABLE(mojo::js::WaitingCallback);

namespace mojo {
namespace js {

namespace {

v8::Handle<v8::String> GetHiddenPropertyName(v8::Isolate* isolate) {
  return gin::StringToSymbol(isolate, "::mojo::js::WaitingCallback");
}

}  // namespace

WaitingCallback::WaitingCallback(v8::Isolate* isolate,
                                 v8::Handle<v8::Function> callback)
      : wait_id_() {
  v8::Handle<v8::Context> context = isolate->GetCurrentContext();
  runner_ = gin::PerContextData::From(context)->runner()->GetWeakPtr();
  GetWrapper(isolate)->SetHiddenValue(GetHiddenPropertyName(isolate), callback);
}

WaitingCallback::~WaitingCallback() {
  DCHECK(!wait_id_) << "Waiting callback was destroyed before being cancelled.";
}

gin::Handle<WaitingCallback> WaitingCallback::Create(
    v8::Isolate* isolate, v8::Handle<v8::Function> callback) {
  return gin::CreateHandle(isolate, new WaitingCallback(isolate, callback));
}

void WaitingCallback::EnsureRegistered(v8::Isolate* isolate) {
  gin::PerIsolateData* data = gin::PerIsolateData::From(isolate);
  if (!data->GetObjectTemplate(&WaitingCallback::kWrapperInfo).IsEmpty()) {
    return;
  }
  v8::Handle<v8::ObjectTemplate> templ = v8::ObjectTemplate::New(isolate);
  templ->SetInternalFieldCount(gin::kNumberOfInternalFields);
  data->SetObjectTemplate(&WaitingCallback::kWrapperInfo, templ);
}

void WaitingCallback::OnHandleReady(MojoResult result) {
  wait_id_ = NULL;

  if (!runner_)
    return;

  gin::Runner::Scope scope(runner_.get());
  v8::Isolate* isolate = runner_->isolate();

  v8::Handle<v8::Value> hidden_value =
      GetWrapper(isolate)->GetHiddenValue(GetHiddenPropertyName(isolate));
  v8::Handle<v8::Function> callback;
  CHECK(gin::ConvertFromV8(isolate, hidden_value, &callback));

  v8::Handle<v8::Value> args[] = { gin::ConvertToV8(isolate, result) };
  runner_->Call(callback, runner_->global(), 1, args);
}

}  // namespace js
}  // namespace mojo
