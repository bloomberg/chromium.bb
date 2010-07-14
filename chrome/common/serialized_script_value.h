// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SERIALIZED_SCRIPT_VALUE_H_
#define CHROME_COMMON_SERIALIZED_SCRIPT_VALUE_H_

#include "base/string16.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSerializedScriptValue.h"

class SerializedScriptValue {
 public:
  SerializedScriptValue();
  SerializedScriptValue(bool is_null, bool is_invalid, const string16& data);
  explicit SerializedScriptValue(const WebKit::WebSerializedScriptValue& value);

  void set_is_null(bool is_null) { is_null_ = is_null; }
  bool is_null() const { return is_null_; }

  void set_is_invalid(bool is_invalid) { is_invalid_ = is_invalid; }
  bool is_invalid() const { return is_invalid_; }

  void set_data(const string16& data) { data_ = data; }
  const string16& data() const { return data_; }

  operator WebKit::WebSerializedScriptValue() const;

 private:
  bool is_null_; // Is this null? If so, none of the other properties are valid.
  bool is_invalid_; // Is data_ valid?
  string16 data_; // The wire string format of the serialized script value.
};

#endif  // CHROME_COMMON_SERIALIZED_SCRIPT_VALUE_H_
