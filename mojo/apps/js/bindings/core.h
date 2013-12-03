// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_APPS_JS_BINDINGS_CORE_H_
#define MOJO_APPS_JS_BINDINGS_CORE_H_

#include "v8/include/v8.h"

namespace mojo {
namespace js {

class Core {
 public:
  static const char kModuleName[];
  static v8::Local<v8::ObjectTemplate> GetTemplate(v8::Isolate* isolate);
};

}  // namespace js
}  // namespace mojo

#endif  // MOJO_APPS_JS_BINDINGS_CORE_H_
