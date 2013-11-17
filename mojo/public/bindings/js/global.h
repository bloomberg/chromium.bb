// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_BINDINGS_JS_MOJO_H_
#define MOJO_PUBLIC_BINDINGS_JS_MOJO_H_

#include "v8/include/v8.h"

namespace mojo {
namespace js {

v8::Local<v8::ObjectTemplate> GetGlobalTemplate(v8::Isolate* isolate);

}  // namespace js
}  // namespace mojo

#endif  // MOJO_PUBLIC_BINDINGS_JS_MOJO_H_
