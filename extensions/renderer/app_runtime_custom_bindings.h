// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_APP_RUNTIME_CUSTOM_BINDINGS_H_
#define EXTENSIONS_RENDERER_APP_RUNTIME_CUSTOM_BINDINGS_H_

#include "extensions/renderer/object_backed_native_handler.h"

namespace extensions {

// The native component of custom bindings for the chrome.app.runtime API.
class AppRuntimeCustomBindings : public ObjectBackedNativeHandler {
 public:
  explicit AppRuntimeCustomBindings(ScriptContext* context);

 private:
  DISALLOW_COPY_AND_ASSIGN(AppRuntimeCustomBindings);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_APP_RUNTIME_CUSTOM_BINDINGS_H_
