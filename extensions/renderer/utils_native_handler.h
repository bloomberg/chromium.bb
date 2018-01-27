// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_UTILS_NATIVE_HANDLER_H_
#define EXTENSIONS_RENDERER_UTILS_NATIVE_HANDLER_H_

#include "base/macros.h"
#include "extensions/renderer/object_backed_native_handler.h"

namespace extensions {
class ScriptContext;

class UtilsNativeHandler : public ObjectBackedNativeHandler {
 public:
  explicit UtilsNativeHandler(ScriptContext* context);
  ~UtilsNativeHandler() override;

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

 private:
  // |args| consists of one argument: an arbitrary value. Returns a deep copy of
  // that value. The copy will have no references to nested values of the
  // argument.
  void DeepCopy(const v8::FunctionCallbackInfo<v8::Value>& args);

  DISALLOW_COPY_AND_ASSIGN(UtilsNativeHandler);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_UTILS_NATIVE_HANDLER_H_
