// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_binding_hooks_test_delegate.h"

namespace extensions {

APIBindingHooksTestDelegate::APIBindingHooksTestDelegate() {}
APIBindingHooksTestDelegate::~APIBindingHooksTestDelegate() {}

bool APIBindingHooksTestDelegate::CreateCustomEvent(
    v8::Local<v8::Context> context,
    const binding::RunJSFunctionSync& run_js_sync,
    const std::string& event_name,
    v8::Local<v8::Value>* event_out) {
  if (!custom_event_.is_null()) {
    *event_out = custom_event_.Run(context, run_js_sync, event_name);
    return true;
  }
  return false;
}

void APIBindingHooksTestDelegate::AddHandler(base::StringPiece name,
                                             const RequestHandler& handler) {
  request_handlers_[name.as_string()] = handler;
}

void APIBindingHooksTestDelegate::SetCustomEvent(
    const CustomEventFactory& custom_event) {
  custom_event_ = custom_event;
}

APIBindingHooks::RequestResult APIBindingHooksTestDelegate::HandleRequest(
    const std::string& method_name,
    const APISignature* signature,
    v8::Local<v8::Context> context,
    std::vector<v8::Local<v8::Value>>* arguments,
    const APITypeReferenceMap& refs) {
  auto iter = request_handlers_.find(method_name);
  if (iter == request_handlers_.end()) {
    return APIBindingHooks::RequestResult(
        APIBindingHooks::RequestResult::NOT_HANDLED);
  }
  return iter->second.Run(signature, context, arguments, refs);
}

}  // namespace extensions
