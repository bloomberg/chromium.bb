// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_RENDERER_SHELL_CUSTOM_BINDINGS_H_
#define APPS_SHELL_RENDERER_SHELL_CUSTOM_BINDINGS_H_

#include "extensions/renderer/object_backed_native_handler.h"

namespace apps {

// Implements custom bindings for the chrome.shell API.
class ShellCustomBindings : public extensions::ObjectBackedNativeHandler {
 public:
  ShellCustomBindings(extensions::ScriptContext* context);

 private:
  void GetView(const v8::FunctionCallbackInfo<v8::Value>& args);

  DISALLOW_COPY_AND_ASSIGN(ShellCustomBindings);
};

}  // namespace apps

#endif  // APPS_SHELL_RENDERER_SHELL_CUSTOM_BINDINGS_H_
