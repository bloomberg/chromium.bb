// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_V8_VALUE_CONVERTER_H_
#define CHROME_RENDERER_V8_VALUE_CONVERTER_H_

#include "v8/include/v8.h"

class Value;
class ListValue;
class DictionaryValue;

// Converts between v8::Value (JavaScript values in the v8 heap) and Chrome's
// values (from base/values.h). Lists and dictionaries are converted
// recursively.
class V8ValueConverter {
 public:
  V8ValueConverter();

  bool allow_date() const { return allow_date_; }
  void set_allow_date(bool val) { allow_date_ = val; }

  bool allow_regexp() const { return allow_regexp_; }
  void set_allow_regexp(bool val) { allow_regexp_ = val; }

  // Converts Value to v8::Value. Only the JSON types (null, boolean, string,
  // number, array, and object) are supported. Other types DCHECK and are
  // replaced with null.
  v8::Handle<v8::Value> ToV8Value(Value* value,
                                  v8::Handle<v8::Context> context);

  // Converts v8::Value to Value. Only the JSON types (null, boolean, string,
  // number, array, and object) are supported by default, but additional types
  // can be supported with setters. If a number fits in int32, we will convert
  // to Value::TYPE_INTEGER instead of Value::TYPE_DOUBLE.
  Value* FromV8Value(v8::Handle<v8::Value> value,
                     v8::Handle<v8::Context> context);

 private:
  v8::Handle<v8::Value> ToV8ValueImpl(Value* value);
  v8::Handle<v8::Value> ToV8Array(ListValue* list);
  v8::Handle<v8::Value> ToV8Object(DictionaryValue* dictionary);

  Value* FromV8ValueImpl(v8::Handle<v8::Value> value);
  ListValue* FromV8Array(v8::Handle<v8::Array> array);
  DictionaryValue* FromV8Object(v8::Handle<v8::Object> object);

  // If true, we will convert Date JavaScript objects to doubles.
  bool allow_date_;

  // If true, we will convet RegExp JavaScript objects to string.
  bool allow_regexp_;
};

#endif // CHROME_RENDERER_V8_VALUE_CONVERTER_H_
