// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_WEBVIEW_CUSTOM_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_WEBVIEW_CUSTOM_BINDINGS_H_

#include "chrome/renderer/extensions/chrome_v8_extension.h"

namespace extensions {

// Implements custom bindings for <webview> operations.
class WebViewCustomBindings : public ChromeV8Extension {
 public:
  WebViewCustomBindings(Dispatcher* dispatcher, ChromeV8Context* context);

 private:
  // Returns a new embedder-unique instance ID for a <webview>.
  // This ID is used to uniquely identify a <webview> element.
  void GetNextInstanceID(const v8::FunctionCallbackInfo<v8::Value>& args);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_WEBVIEW_CUSTOM_BINDINGS_H_
