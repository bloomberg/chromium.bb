// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_APP_WINDOW_CUSTOM_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_APP_WINDOW_CUSTOM_BINDINGS_H_
#pragma once

#include "chrome/renderer/extensions/chrome_v8_extension.h"

class ExtensionDispatcher;

namespace extensions {

// Implements custom bindings for the appWindow API.
class AppWindowCustomBindings : public ChromeV8Extension {
 public:
  explicit AppWindowCustomBindings(ExtensionDispatcher* extension_dispatcher);

 private:
  static v8::Handle<v8::Value> GetView(const v8::Arguments& args);

  DISALLOW_COPY_AND_ASSIGN(AppWindowCustomBindings);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_APP_WINDOW_CUSTOM_BINDINGS_H_
