// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_RENDERER_SHELL_CUSTOM_BINDINGS_H_
#define EXTENSIONS_SHELL_RENDERER_SHELL_CUSTOM_BINDINGS_H_

#include "extensions/renderer/object_backed_native_handler.h"

namespace extensions {

// Implements custom bindings for the chrome.shell API.
class ShellCustomBindings : public ObjectBackedNativeHandler {
 public:
  ShellCustomBindings(ScriptContext* context);

 private:
  void GetView(const v8::FunctionCallbackInfo<v8::Value>& args);

  DISALLOW_COPY_AND_ASSIGN(ShellCustomBindings);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_RENDERER_SHELL_CUSTOM_BINDINGS_H_
