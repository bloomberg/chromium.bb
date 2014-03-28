// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_BINDINGS_JS_HANDLE_H_
#define MOJO_BINDINGS_JS_HANDLE_H_

#include "gin/converter.h"
#include "mojo/public/cpp/system/core.h"

namespace gin {

template<>
struct Converter<mojo::Handle> {
  static v8::Handle<v8::Value> ToV8(v8::Isolate* isolate,
                                    const mojo::Handle& val);
  static bool FromV8(v8::Isolate* isolate, v8::Handle<v8::Value> val,
                     mojo::Handle* out);
};

}  // namespace gin

#endif  // MOJO_BINDINGS_JS_HANDLE_H_
