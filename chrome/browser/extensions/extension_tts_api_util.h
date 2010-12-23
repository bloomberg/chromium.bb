// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TTS_API_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TTS_API_UTIL_H_

#include <string>

#include "base/values.h"

namespace extension_tts_api_util {

extern const char kVoiceNameKey[];
extern const char kLocaleKey[];
extern const char kGenderKey[];
extern const char kRateKey[];
extern const char kPitchKey[];
extern const char kVolumeKey[];
extern const char kEnqueueKey[];

bool ReadNumberByKey(DictionaryValue* dict,
                     const char* key,
                     double* ret_value);

}  // namespace extension_tts_api_util.
#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_TTS_API_UTIL_H_
