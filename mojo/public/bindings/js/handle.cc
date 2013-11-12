// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/js/handle.h"

namespace gin {

v8::Handle<v8::Value> Converter<mojo::Handle>::ToV8(v8::Isolate* isolate,
                                                    mojo::Handle val) {
  return Converter<MojoHandle>::ToV8(isolate, val.value);
}

bool Converter<mojo::Handle>::FromV8(v8::Handle<v8::Value> val,
                                     mojo::Handle* out) {
  return Converter<MojoHandle>::FromV8(val, &out->value);
}

}  // namespace gin
