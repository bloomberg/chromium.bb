// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_PAGE_ACTIONS_CUSTOM_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_PAGE_ACTIONS_CUSTOM_BINDINGS_H_

#include "extensions/renderer/object_backed_native_handler.h"

namespace extensions {
class Dispatcher;

// Implements custom bindings for the pageActions API.
class PageActionsCustomBindings : public ObjectBackedNativeHandler {
 public:
  PageActionsCustomBindings(Dispatcher* dispatcher, ScriptContext* context);

 private:
  void GetCurrentPageActions(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Dispatcher handle. Not owned.
  Dispatcher* dispatcher_;
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_PAGE_ACTIONS_CUSTOM_BINDINGS_H_
