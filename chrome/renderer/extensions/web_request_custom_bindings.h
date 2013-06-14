// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_WEB_REQUEST_CUSTOM_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_WEB_REQUEST_CUSTOM_BINDINGS_H_

#include "chrome/renderer/extensions/chrome_v8_extension.h"

namespace extensions {

// Implements custom bindings for the webRequest API.
class WebRequestCustomBindings : public ChromeV8Extension {
 public:
  WebRequestCustomBindings(Dispatcher* dispatcher, ChromeV8Context* context);

 private:
  void GetUniqueSubEventName(const v8::FunctionCallbackInfo<v8::Value>& args);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_WEB_REQUEST_CUSTOM_BINDINGS_H_
