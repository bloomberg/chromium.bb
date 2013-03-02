// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_NATIVE_HANDLER_H_
#define CHROME_RENDERER_EXTENSIONS_NATIVE_HANDLER_H_

#include "v8/include/v8.h"

namespace extensions {

// NativeHandlers are intended to be used with a ModuleSystem. The ModuleSystem
// will assume ownership of the NativeHandler, and as a ModuleSystem is tied to
// a single v8::Context, this implies that NativeHandlers will also be tied to
// a single v8::Context.
// TODO(koz): Rename this to NativeJavaScriptModule.
class NativeHandler {
 public:
  virtual ~NativeHandler() {}

  // Create a new instance of the object this handler specifies.
  virtual v8::Handle<v8::Object> NewInstance() = 0;
};

}  // extensions

#endif  // CHROME_RENDERER_EXTENSIONS_NATIVE_HANDLER_H_
