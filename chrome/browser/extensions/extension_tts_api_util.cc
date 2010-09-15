// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tts_api_util.h"

namespace extension_tts_api_util {

// Static.
bool ReadNumberByKey(DictionaryValue* dict,
                     const char* key,
                     double* ret_value) {
  Value* value;
  if (!dict->Get(key, &value))
    return false;

  if (value->IsType(Value::TYPE_INTEGER)) {
    int int_value;
    if (!dict->GetInteger(key, &int_value))
      return false;
    *ret_value = int_value;
  } else if (value->IsType(Value::TYPE_REAL)) {
    if (!dict->GetReal(key, ret_value))
      return false;
  } else {
    return false;
  }
  return true;
}

// Static.
void AppendSpeakOption(std::string key,
                       std::string value,
                       std::string* options) {
  *options += key + kEqualStr + value + kDelimiter;
}

}  // namespace extension_tts_api_util.
