// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_EXTENSION_API_CONSTANTS_H_
#define CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_EXTENSION_API_CONSTANTS_H_
#pragma once

#include <string>

#include "base/values.h"

namespace tts_extension_api_constants {

extern const char kVoiceNameKey[];
extern const char kLangKey[];
extern const char kGenderKey[];
extern const char kRateKey[];
extern const char kPitchKey[];
extern const char kVolumeKey[];
extern const char kEnqueueKey[];
extern const char kEventTypeKey[];
extern const char kEventTypesKey[];
extern const char kCharIndexKey[];
extern const char kErrorMessageKey[];
extern const char kRequiredEventTypesKey[];
extern const char kDesiredEventTypesKey[];
extern const char kExtensionIdKey[];
extern const char kSrcIdKey[];
extern const char kIsFinalEventKey[];
extern const char kOnEventKey[];

extern const char kGenderFemale[];
extern const char kGenderMale[];

extern const char kEventTypeStart[];
extern const char kEventTypeEnd[];
extern const char kEventTypeWord[];
extern const char kEventTypeSentence[];
extern const char kEventTypeMarker[];
extern const char kEventTypeInterrupted[];
extern const char kEventTypeCancelled[];
extern const char kEventTypeError[];

extern const char kNativeVoiceName[];

extern const char kErrorUndeclaredEventType[];
extern const char kErrorUtteranceTooLong[];
extern const char kErrorInvalidLang[];
extern const char kErrorInvalidGender[];
extern const char kErrorInvalidRate[];
extern const char kErrorInvalidPitch[];
extern const char kErrorInvalidVolume[];

}  // namespace tts_extension_api_constants.
#endif  // CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_EXTENSION_API_CONSTANTS_H_
