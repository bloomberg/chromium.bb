// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_EXTENSION_CUSTOM_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_EXTENSION_CUSTOM_BINDINGS_H_
#pragma once

#include "chrome/renderer/extensions/chrome_v8_extension.h"

class ExtensionDispatcher;

namespace extensions {

// Implements custom bindings for the extension API.
class ExtensionCustomBindings : public ChromeV8Extension {
 public:
  explicit ExtensionCustomBindings(ExtensionDispatcher* extension_dispatcher);

 private:
  static v8::Handle<v8::Value> GetExtensionViews(const v8::Arguments& args);

  // Creates a new messaging channel to the given extension.
  static v8::Handle<v8::Value> OpenChannelToExtension(
      const v8::Arguments& args);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_EXTENSION_CUSTOM_BINDINGS_H_
