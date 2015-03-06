// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/script_injection_callback.h"

#include "extensions/renderer/script_injection.h"

namespace extensions {

ScriptInjectionCallback::ScriptInjectionCallback(
    ScriptInjection* injection,
    blink::WebLocalFrame* web_frame)
    : injection_(injection),
      web_frame_(web_frame) {
}

ScriptInjectionCallback::~ScriptInjectionCallback() {
}

void ScriptInjectionCallback::completed(
    const blink::WebVector<v8::Local<v8::Value> >& result) {
  injection_->OnJsInjectionCompleted(web_frame_, result);
  delete this;
}

}  // namespace extensions
