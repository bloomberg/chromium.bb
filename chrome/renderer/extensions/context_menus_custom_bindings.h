// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_CONTEXT_MENUS_CUSTOM_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_CONTEXT_MENUS_CUSTOM_BINDINGS_H_

#include "chrome/renderer/extensions/chrome_v8_extension.h"

namespace extensions {

// Implements custom bindings for the contextMenus API.
class ContextMenusCustomBindings : public ChromeV8Extension {
 public:
  ContextMenusCustomBindings(Dispatcher* dispatcher,
                             v8::Handle<v8::Context> v8_context);
};

}  // extensions

#endif  // CHROME_RENDERER_EXTENSIONS_CONTEXT_MENUS_CUSTOM_BINDINGS_H_
