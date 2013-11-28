// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/activity_log_converter_strategy.h"
#include "base/values.h"

namespace extensions {

bool ActivityLogConverterStrategy::FromV8Object(v8::Handle<v8::Object> value,
                                                base::Value** out,
                                                v8::Isolate* isolate) const {
  return FromV8ObjectInternal(value, out, isolate);
}

bool ActivityLogConverterStrategy::FromV8Array(v8::Handle<v8::Array> value,
                                               base::Value** out,
                                               v8::Isolate* isolate) const {
  return FromV8ObjectInternal(value, out, isolate);
}

bool ActivityLogConverterStrategy::FromV8ObjectInternal(
    v8::Handle<v8::Object> value,
    base::Value** out,
    v8::Isolate* isolate) const {

  // Handle JSObject.
  // We cannot use value->Get(key/index) as there may be a getter method,
  // accessor, interceptor/handler, or access check callback set on the
  // property. If that it is the case, any of those may invoke JS code that
  // may result on logging extension activity events caused by value conversion
  // rather than extension itself.

  // V8 arrays are handled here in the same way as other JSObjects as they may
  // also have getter methods, accessor, interceptor/handler, and access check
  // callback.

  v8::Handle<v8::String> name = v8::String::NewFromUtf8(isolate, "[");
  if (value->IsFunction()) {
    name =
        v8::String::Concat(name, v8::String::NewFromUtf8(isolate, "Function"));
    v8::Handle<v8::Value> fname =
        v8::Handle<v8::Function>::Cast(value)->GetName();
    if (fname->IsString() && v8::Handle<v8::String>::Cast(fname)->Length()) {
      name = v8::String::Concat(name, v8::String::NewFromUtf8(isolate, " "));
      name = v8::String::Concat(name, v8::Handle<v8::String>::Cast(fname));
      name = v8::String::Concat(name, v8::String::NewFromUtf8(isolate, "()"));
    }
  } else {
    name = v8::String::Concat(name, value->GetConstructorName());
  }
  name = v8::String::Concat(name, v8::String::NewFromUtf8(isolate, "]"));
  *out = new base::StringValue(std::string(*v8::String::Utf8Value(name)));
  // Prevent V8ValueConverter from further processing this object.
  return true;
}

}  // namespace extensions
