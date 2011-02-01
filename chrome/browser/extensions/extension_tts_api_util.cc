// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tts_api_util.h"

namespace extension_tts_api_util {

const char kVoiceNameKey[] = "voiceName";
const char kLocaleKey[] = "locale";
const char kGenderKey[] = "gender";
const char kRateKey[] = "rate";
const char kPitchKey[] = "pitch";
const char kVolumeKey[] = "volume";
const char kEnqueueKey[] = "enqueue";

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
  } else if (value->IsType(Value::TYPE_DOUBLE)) {
    if (!dict->GetDouble(key, ret_value))
      return false;
  } else {
    return false;
  }
  return true;
}

}  // namespace extension_tts_api_util.
