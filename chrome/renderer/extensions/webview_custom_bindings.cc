// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/webview_custom_bindings.h"

#include <string>

#include "base/bind.h"
#include "v8/include/v8.h"

namespace extensions {

WebViewCustomBindings::WebViewCustomBindings(
    Dispatcher* dispatcher, ChromeV8Context* context)
    : ChromeV8Extension(dispatcher, context) {
  RouteFunction("GetNextInstanceID",
      base::Bind(&WebViewCustomBindings::GetNextInstanceID,
                 base::Unretained(this)));
}

void WebViewCustomBindings::GetNextInstanceID(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  static int next_instance_id = 1;
  args.GetReturnValue().Set(next_instance_id++);
}

}  // namespace extensions
