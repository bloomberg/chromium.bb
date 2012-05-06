// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_V8_VALUE_CONVERTER_H_
#define CONTENT_PUBLIC_RENDERER_V8_VALUE_CONVERTER_H_

#include "content/common/content_export.h"
#include "v8/include/v8.h"

namespace base {
class Value;
}

namespace content {

// Converts between v8::Value (JavaScript values in the v8 heap) and Chrome's
// values (from base/values.h). Lists and dictionaries are converted
// recursively.
//
// The JSON types (null, boolean, string, number, array, and object) as well as
// binary values are supported. For binary values, we convert to WebKit
// ArrayBuffers, and support converting from an ArrayBuffer or any of the
// ArrayBufferView subclasses (Uint8Array, etc.).
class CONTENT_EXPORT V8ValueConverter {
 public:
  static V8ValueConverter* create();

  virtual ~V8ValueConverter() {}

  // Converts Value to v8::Value. Unsupported types are replaced with null.
  // If an array or object throws while setting a value, that property or item
  // is skipped, leaving a hole in the case of arrays.
  virtual v8::Handle<v8::Value> ToV8Value(
      const base::Value* value,
      v8::Handle<v8::Context> context) const = 0;

  // Converts v8::Value to Value. Unsupported types are replaced with null.
  // If an array or object throws while getting a value, that property or item
  // is replaced with null.
  virtual base::Value* FromV8Value(v8::Handle<v8::Value> value,
                                   v8::Handle<v8::Context> context) const = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_V8_VALUE_CONVERTER_H_
