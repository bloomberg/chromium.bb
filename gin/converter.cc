// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/converter.h"

#include "v8/include/v8.h"

using v8::Boolean;
using v8::Function;
using v8::Handle;
using v8::Integer;
using v8::Isolate;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Value;

namespace gin {

Handle<Value> Converter<bool>::ToV8(Isolate* isolate, bool val) {
  return Boolean::New(val).As<Value>();
}

bool Converter<bool>::FromV8(Handle<Value> val, bool* out) {
  *out = val->BooleanValue();
  return true;
}

Handle<Value> Converter<int32_t>::ToV8(Isolate* isolate, int32_t val) {
  return Integer::New(val, isolate).As<Value>();
}

bool Converter<int32_t>::FromV8(Handle<Value> val, int32_t* out) {
  if (!val->IsInt32())
    return false;
  *out = val->Int32Value();
  return true;
}

Handle<Value> Converter<uint32_t>::ToV8(Isolate* isolate, uint32_t val) {
  return Integer::NewFromUnsigned(val, isolate).As<Value>();
}

bool Converter<uint32_t>::FromV8(Handle<Value> val, uint32_t* out) {
  if (!val->IsUint32())
    return false;
  *out = val->Uint32Value();
  return true;
}

Handle<Value> Converter<double>::ToV8(Isolate* isolate, double val) {
  return Number::New(isolate, val).As<Value>();
}

bool Converter<double>::FromV8(Handle<Value> val, double* out) {
  if (!val->IsNumber())
    return false;
  *out = val->NumberValue();
  return true;
}

Handle<Value> Converter<std::string>::ToV8(Isolate* isolate,
                                           const std::string& val) {
  return String::NewFromUtf8(isolate,
                             val.data(),
                             String::kNormalString,
                             val.length());
}

bool Converter<std::string>::FromV8(Handle<Value> val,
                                    std::string* out) {
  if (!val->IsString())
    return false;
  Handle<String> str = Handle<String>::Cast(val);
  int length = str->Utf8Length();
  out->resize(length);
  str->WriteUtf8(&(*out)[0], length, NULL, String::NO_NULL_TERMINATION);
  return true;
}

bool Converter<Handle<Function> >::FromV8(Handle<Value> val,
                                          Handle<Function>* out) {
  if (!val->IsFunction())
    return false;
  *out = Handle<Function>::Cast(val);
  return true;
}

Handle<Value> Converter<Handle<Object> >::ToV8(Handle<Object> val) {
  return val.As<Value>();
}

bool Converter<Handle<Object> >::FromV8(Handle<Value> val,
                                        Handle<Object>* out) {
  if (!val->IsObject())
    return false;
  *out = Handle<Object>::Cast(val);
  return true;
}

}  // namespace gin
