// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_V8_VALUE_CONVERTER_H_
#define CHROME_RENDERER_V8_VALUE_CONVERTER_H_

#include "content/common/content_export.h"
#include "v8/include/v8.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

// Converts between v8::Value (JavaScript values in the v8 heap) and Chrome's
// values (from base/values.h). Lists and dictionaries are converted
// recursively.
//
// Only the JSON types (null, boolean, string, number, array, and object) are
// supported by default. Additional types can be allowed with e.g.
// set_allow_date(), set_allow_regexp().
class CONTENT_EXPORT V8ValueConverter {
 public:
  V8ValueConverter();

  bool allow_undefined() const { return allow_undefined_; }
  void set_allow_undefined(bool val) { allow_undefined_ = val; }

  bool allow_date() const { return allow_date_; }
  void set_allow_date(bool val) { allow_date_ = val; }

  bool allow_regexp() const { return allow_regexp_; }
  void set_allow_regexp(bool val) { allow_regexp_ = val; }

  // Converts Value to v8::Value. Unsupported types are replaced with null.
  // If an array or object throws while setting a value, that property or item
  // is skipped, leaving a hole in the case of arrays.
  v8::Handle<v8::Value> ToV8Value(base::Value* value,
                                  v8::Handle<v8::Context> context) const;

  // Converts v8::Value to Value. Unsupported types are replaced with null.
  // If an array or object throws while getting a value, that property or item
  // is replaced with null.
  base::Value* FromV8Value(v8::Handle<v8::Value> value,
                           v8::Handle<v8::Context> context) const;

 private:
  v8::Handle<v8::Value> ToV8ValueImpl(base::Value* value) const;
  v8::Handle<v8::Value> ToV8Array(base::ListValue* list) const;
  v8::Handle<v8::Value> ToV8Object(base::DictionaryValue* dictionary) const;

  base::Value* FromV8ValueImpl(v8::Handle<v8::Value> value) const;
  base::ListValue* FromV8Array(v8::Handle<v8::Array> array) const;
  base::DictionaryValue* FromV8Object(v8::Handle<v8::Object> object) const;

  // If true, we will convert undefined JavaScript values to null.
  bool allow_undefined_;

  // If true, we will convert Date JavaScript objects to doubles.
  bool allow_date_;

  // If true, we will convet RegExp JavaScript objects to string.
  bool allow_regexp_;
};

#endif // CHROME_RENDERER_V8_VALUE_CONVERTER_H_
