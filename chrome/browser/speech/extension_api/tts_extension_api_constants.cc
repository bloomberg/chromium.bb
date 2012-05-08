// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/extension_api/tts_extension_api_constants.h"

namespace tts_extension_api_constants {

const char kVoiceNameKey[] = "voiceName";
const char kLangKey[] = "lang";
const char kGenderKey[] = "gender";
const char kRateKey[] = "rate";
const char kPitchKey[] = "pitch";
const char kVolumeKey[] = "volume";
const char kEnqueueKey[] = "enqueue";
const char kEventTypeKey[] = "type";
const char kEventTypesKey[] = "eventTypes";
const char kCharIndexKey[] = "charIndex";
const char kErrorMessageKey[] = "errorMessage";
const char kRequiredEventTypesKey[] = "requiredEventTypes";
const char kDesiredEventTypesKey[] = "desiredEventTypes";
const char kExtensionIdKey[] = "extensionId";
const char kSrcIdKey[] = "srcId";
const char kIsFinalEventKey[] = "isFinalEvent";
const char kOnEventKey[] = "onEvent";

const char kGenderFemale[] = "female";
const char kGenderMale[] = "male";

const char kEventTypeStart[] = "start";
const char kEventTypeEnd[] = "end";
const char kEventTypeWord[] = "word";
const char kEventTypeSentence[] = "sentence";
const char kEventTypeMarker[] = "marker";
const char kEventTypeInterrupted[] = "interrupted";
const char kEventTypeCancelled[] = "cancelled";
const char kEventTypeError[] = "error";

const char kNativeVoiceName[] = "native";

const char kErrorUndeclaredEventType[] =
    "Cannot send an event type that is not declared in the extension manifest.";
const char kErrorUtteranceTooLong[] = "Utterance length is too long.";
const char kErrorInvalidLang[] = "Invalid lang.";
const char kErrorInvalidGender[] = "Invalid gender.";
const char kErrorInvalidRate[] = "Invalid rate.";
const char kErrorInvalidPitch[] = "Invalid pitch.";
const char kErrorInvalidVolume[] = "Invalid volume.";

}  // namespace tts_extension_api_constants.
