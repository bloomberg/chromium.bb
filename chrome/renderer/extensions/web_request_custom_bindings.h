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
  WebRequestCustomBindings(Dispatcher* dispatcher,
                           v8::Handle<v8::Context> v8_context);

 private:
  v8::Handle<v8::Value> GetUniqueSubEventName(const v8::Arguments& args);
};

}  // extensions

#endif  // CHROME_RENDERER_EXTENSIONS_WEB_REQUEST_CUSTOM_BINDINGS_H_
