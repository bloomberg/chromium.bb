// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_CONTEXT_MENUS_CUSTOM_BINDINGS_H_
#define EXTENSIONS_RENDERER_CONTEXT_MENUS_CUSTOM_BINDINGS_H_

#include "extensions/renderer/object_backed_native_handler.h"

namespace extensions {
class ScriptContext;

// Implements custom bindings for the contextMenus API.
class ContextMenusCustomBindings : public ObjectBackedNativeHandler {
 public:
  explicit ContextMenusCustomBindings(ScriptContext* context);

  // ObjectBackedNativeHandler:
  void AddRoutes() override;
};

}  // extensions

#endif  // EXTENSIONS_RENDERER_CONTEXT_MENUS_CUSTOM_BINDINGS_H_
