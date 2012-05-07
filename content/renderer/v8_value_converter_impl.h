// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_V8_VALUE_CONVERTER_IMPL_H_
#define CONTENT_RENDERER_V8_VALUE_CONVERTER_IMPL_H_

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "content/public/renderer/v8_value_converter.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

class CONTENT_EXPORT V8ValueConverterImpl : public content::V8ValueConverter {
 public:
  V8ValueConverterImpl();

  // Use the following setters to support additional types other than the
  // default ones.
  bool allow_undefined() const { return allow_undefined_; }
  void set_allow_undefined(bool val) { allow_undefined_ = val; }

  bool allow_date() const { return allow_date_; }
  void set_allow_date(bool val) { allow_date_ = val; }

  bool allow_regexp() const { return allow_regexp_; }
  void set_allow_regexp(bool val) { allow_regexp_ = val; }

  // V8ValueConverter implementation.
  virtual v8::Handle<v8::Value> ToV8Value(
      const base::Value* value,
      v8::Handle<v8::Context> context) const OVERRIDE;
  virtual base::Value* FromV8Value(
      v8::Handle<v8::Value> value,
      v8::Handle<v8::Context> context) const OVERRIDE;

 private:
  v8::Handle<v8::Value> ToV8ValueImpl(const base::Value* value) const;
  v8::Handle<v8::Value> ToV8Array(const base::ListValue* list) const;
  v8::Handle<v8::Value> ToV8Object(
      const base::DictionaryValue* dictionary) const;

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

#endif  // CONTENT_RENDERER_V8_VALUE_CONVERTER_IMPL_H_
