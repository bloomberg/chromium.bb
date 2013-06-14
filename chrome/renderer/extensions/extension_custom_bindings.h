// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_EXTENSION_CUSTOM_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_EXTENSION_CUSTOM_BINDINGS_H_

#include "chrome/renderer/extensions/chrome_v8_extension.h"

namespace extensions {
class Dispatcher;

// Implements custom bindings for the extension API.
class ExtensionCustomBindings : public ChromeV8Extension {
 public:
  explicit ExtensionCustomBindings(Dispatcher* dispatcher,
                                   ChromeV8Context* context);

 private:
  void GetExtensionViews(const v8::FunctionCallbackInfo<v8::Value>& args);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_EXTENSION_CUSTOM_BINDINGS_H_
