// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_V8_VALUE_CONVERTER_IMPL_H_
#define CONTENT_RENDERER_V8_VALUE_CONVERTER_IMPL_H_

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "content/public/renderer/v8_value_converter.h"

namespace base {
class BinaryValue;
class DictionaryValue;
class ListValue;
class Value;
}

class CONTENT_EXPORT V8ValueConverterImpl : public content::V8ValueConverter {
 public:
  V8ValueConverterImpl();

  // V8ValueConverter implementation.
  virtual bool GetUndefinedAllowed() const OVERRIDE;
  virtual void SetUndefinedAllowed(bool val) OVERRIDE;
  virtual bool GetDateAllowed() const OVERRIDE;
  virtual void SetDateAllowed(bool val) OVERRIDE;
  virtual bool GetRegexpAllowed() const OVERRIDE;
  virtual void SetRegexpAllowed(bool val) OVERRIDE;
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
  v8::Handle<v8::Value> ToArrayBuffer(const base::BinaryValue* value) const;

  base::Value* FromV8ValueImpl(v8::Handle<v8::Value> value) const;
  base::ListValue* FromV8Array(v8::Handle<v8::Array> array) const;

  // This will convert objects of type ArrayBuffer or any of the
  // ArrayBufferView subclasses. The return value will be NULL if |value| is
  // not one of these types.
  base::BinaryValue* FromV8Buffer(v8::Handle<v8::Value> value) const;

  base::DictionaryValue* FromV8Object(v8::Handle<v8::Object> object) const;

  // If true, we will convert undefined JavaScript values to null.
  bool undefined_allowed_;

  // If true, we will convert Date JavaScript objects to doubles.
  bool date_allowed_;

  // If true, we will convet RegExp JavaScript objects to string.
  bool regexp_allowed_;
};

#endif  // CONTENT_RENDERER_V8_VALUE_CONVERTER_IMPL_H_
