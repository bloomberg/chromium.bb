// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_ID_GENERATOR_CUSTOM_BINDINGS_H_
#define EXTENSIONS_RENDERER_ID_GENERATOR_CUSTOM_BINDINGS_H_

#include "extensions/renderer/object_backed_native_handler.h"

namespace extensions {
class ScriptContext;

// Implements function that can be used by JS layer to generate unique integer
// identifiers.
class IdGeneratorCustomBindings : public ObjectBackedNativeHandler {
 public:
  IdGeneratorCustomBindings(ScriptContext* context);

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

 private:
  void GetNextId(const v8::FunctionCallbackInfo<v8::Value>& args);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_ID_GENERATOR_CUSTOM_BINDINGS_H_
