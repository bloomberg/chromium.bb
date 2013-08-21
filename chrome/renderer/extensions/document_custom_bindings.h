// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_DOCUMENT_CUSTOM_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_DOCUMENT_CUSTOM_BINDINGS_H_

#include "chrome/renderer/extensions/chrome_v8_extension.h"

namespace extensions {

// Implements custom bindings for document-level operations.
class DocumentCustomBindings : public ChromeV8Extension {
 public:
  DocumentCustomBindings(Dispatcher* dispatcher, ChromeV8Context* context);

 private:
  // Registers the provided element as a custom element in Blink.
  void RegisterElement(const v8::FunctionCallbackInfo<v8::Value>& args);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_DOCUMENT_CUSTOM_BINDINGS_H_
