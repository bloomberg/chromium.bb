// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_ID_GENERATOR_CUSTOM_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_ID_GENERATOR_CUSTOM_BINDINGS_H_

#include "chrome/renderer/extensions/chrome_v8_extension.h"

namespace extensions {

// Implements function that can be used by JS layer to generate unique integer
// identifiers.
class IdGeneratorCustomBindings : public ChromeV8Extension {
 public:
  IdGeneratorCustomBindings(Dispatcher* dispatcher, ChromeV8Context* context);

 private:
  void GetNextId(const v8::FunctionCallbackInfo<v8::Value>& args);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_ID_GENERATOR_CUSTOM_BINDINGS_H_
