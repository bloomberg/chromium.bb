// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/serialized_script_value.h"

#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSerializedScriptValue.h"

using WebKit::WebSerializedScriptValue;

namespace content {

SerializedScriptValue::SerializedScriptValue()
    : is_null_(true),
      is_invalid_(false) {
}

SerializedScriptValue::SerializedScriptValue(
    bool is_null, bool is_invalid, const string16& data)
    : is_null_(is_null),
      is_invalid_(is_invalid),
      data_(data) {
}

SerializedScriptValue::SerializedScriptValue(
    const WebSerializedScriptValue& value) {
  set_web_serialized_script_value(value);
}

SerializedScriptValue::operator WebSerializedScriptValue() const {
  if (is_null_)
    return WebSerializedScriptValue();
  if (is_invalid_)
    return WebSerializedScriptValue::createInvalid();
  return WebSerializedScriptValue::fromString(data_);
}

void SerializedScriptValue::set_web_serialized_script_value(
    const WebSerializedScriptValue& value) {
  is_null_ = value.isNull();
  is_invalid_ = value.isNull() ? false : value.toString().isNull();
  data_ = value.isNull() ? string16() : static_cast<string16>(value.toString());
}

}  // namespace content
