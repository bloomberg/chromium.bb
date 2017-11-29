// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/renderer/headless_tab_socket_bindings.h"

#include "headless/lib/renderer/headless_render_frame_controller_impl.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "v8/include/v8-inspector.h"

namespace headless {

HeadlessTabSocketBindings::HeadlessTabSocketBindings(
    HeadlessRenderFrameControllerImpl* parent_controller,
    content::RenderFrame* render_frame,
    v8::Local<v8::Context> context,
    int world_id)
    : parent_controller_(parent_controller),
      render_frame_(render_frame),
      context_(blink::MainThreadIsolate(), context),
      world_id_(world_id),
      installed_(false) {}

HeadlessTabSocketBindings::~HeadlessTabSocketBindings() = default;

bool HeadlessTabSocketBindings::InitializeTabSocketBindings() {
  if (installed_)
    return false;

  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  if (context_.IsEmpty())
    return false;

  v8::Local<v8::Context> context = context_.Get(blink::MainThreadIsolate());
  v8::Context::Scope context_scope(context);
  gin::Handle<HeadlessTabSocketBindings> bindings =
      gin::CreateHandle(isolate, this);
  if (bindings.IsEmpty())
    return false;

  v8::Local<v8::Object> global = context->Global();
  global->Set(gin::StringToV8(isolate, "TabSocket"), bindings.ToV8());
  installed_ = true;
  return true;
}

void HeadlessTabSocketBindings::OnMessageFromEmbedder(
    const std::string& message) {
  if (on_message_callback_.IsEmpty()) {
    pending_messages_.push_back(message);
    return;
  }

  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);
  v8::Local<v8::Value> argv[] = {
      gin::Converter<std::string>::ToV8(isolate, message),
  };

  render_frame_->GetWebFrame()->RequestExecuteV8Function(
      context, GetOnMessageCallback(), context->Global(), arraysize(argv), argv,
      this);
}

gin::ObjectTemplateBuilder HeadlessTabSocketBindings::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<HeadlessTabSocketBindings>::GetObjectTemplateBuilder(
             isolate)
      .SetMethod("send", &HeadlessTabSocketBindings::SendImpl)
      .SetProperty("onmessage", &HeadlessTabSocketBindings::GetOnMessage,
                   &HeadlessTabSocketBindings::SetOnMessage);
}

void HeadlessTabSocketBindings::SendImpl(const std::string& message) {
  v8::Local<v8::Context> context = context_.Get(blink::MainThreadIsolate());
  int execution_context_id =
      v8_inspector::V8ContextInfo::executionContextId(context);
  parent_controller_->EnsureTabSocketPtr()->SendMessageToEmbedder(
      message, execution_context_id);
}

void HeadlessTabSocketBindings::SetOnMessage(v8::Local<v8::Function> callback) {
  on_message_callback_.Reset(blink::MainThreadIsolate(), callback);
  for (const std::string& message : pending_messages_) {
    OnMessageFromEmbedder(message);
  }
  pending_messages_.clear();
}

v8::Local<v8::Function> HeadlessTabSocketBindings::GetOnMessageCallback() {
  return v8::Local<v8::Function>::New(blink::MainThreadIsolate(),
                                      on_message_callback_);
}

gin::WrapperInfo HeadlessTabSocketBindings::kWrapperInfo = {
    gin::kEmbedderNativeGin};

}  // namespace headless
