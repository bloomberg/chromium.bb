// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/css_native_handler.h"

#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebScriptBindings.h"
#include "third_party/WebKit/public/web/WebSelector.h"

namespace extensions {

using WebKit::WebString;

CssNativeHandler::CssNativeHandler(ChromeV8Context* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction("CanonicalizeCompoundSelector",
                base::Bind(&CssNativeHandler::CanonicalizeCompoundSelector,
                           base::Unretained(this)));
}

void CssNativeHandler::CanonicalizeCompoundSelector(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsString());
  WebString input_selector =
      WebKit::WebScriptBindings::toWebString(args[0].As<v8::String>());
  WebString output_selector = WebKit::canonicalizeSelector(
      input_selector, WebKit::WebSelectorTypeCompound);
  args.GetReturnValue().Set(WebKit::WebScriptBindings::toV8String(
      output_selector, context()->v8_context()->GetIsolate()));
}

}  // namespace extensions
