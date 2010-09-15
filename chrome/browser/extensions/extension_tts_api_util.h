// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TTS_API_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TTS_API_UTIL_H_

#include <string>

#include "base/values.h"

namespace extension_tts_api_util {

const char kNameKey[] = "name";
const char kLanguageNameKey[] = "languageName";
const char kGenderKey[] = "gender";
const char kRateKey[] = "rate";
const char kPitchKey[] = "pitch";
const char kVolumeKey[] = "volume";
const char kEqualStr[] = "=";
const char kDelimiter[] = ";";

bool ReadNumberByKey(DictionaryValue* dict,
                     const char* key,
                     double* ret_value);

void AppendSpeakOption(std::string key,
                       std::string value,
                       std::string* options);

}  // namespace extension_tts_api_util.
#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_TTS_API_UTIL_H_
