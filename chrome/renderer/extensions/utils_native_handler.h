// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_UTILS_NATIVE_HANDLER_H_
#define CHROME_RENDERER_EXTENSIONS_UTILS_NATIVE_HANDLER_H_

#include "extensions/renderer/object_backed_native_handler.h"

namespace extensions {
class ChromeV8Context;

class UtilsNativeHandler : public ObjectBackedNativeHandler {
 public:
  explicit UtilsNativeHandler(ChromeV8Context* context);
  virtual ~UtilsNativeHandler();

 private:
  // |args| consists of two arguments: a public class name, and a reference
  // to the implementation class. CreateClassWrapper returns a new class
  // that wraps the implementation, while hiding its members.
  void CreateClassWrapper(const v8::FunctionCallbackInfo<v8::Value>& args);

  DISALLOW_COPY_AND_ASSIGN(UtilsNativeHandler);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_UTILS_NATIVE_HANDLER_H_
