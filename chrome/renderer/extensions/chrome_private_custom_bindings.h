// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_CHROME_PRIVATE_CUSTOM_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_CHROME_PRIVATE_CUSTOM_BINDINGS_H_
#pragma once

#include "chrome/renderer/extensions/chrome_v8_extension.h"

class ExtensionDispatcher;

namespace extensions {

// Implements custom bindings for the chromePrivate API.
class ChromePrivateCustomBindings : public ChromeV8Extension {
 public:
  ChromePrivateCustomBindings(
      int dependency_count,
      const char** dependencies,
      ExtensionDispatcher* extension_dispatcher);

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) OVERRIDE;

 private:
  // Decodes supplied JPEG byte array to image pixel array.
  static v8::Handle<v8::Value> DecodeJPEG(const v8::Arguments& args);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_CHROME_PRIVATE_CUSTOM_BINDINGS_H_
