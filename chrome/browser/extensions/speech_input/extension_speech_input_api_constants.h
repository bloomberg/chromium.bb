// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SPEECH_INPUT_EXTENSION_SPEECH_INPUT_API_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_SPEECH_INPUT_EXTENSION_SPEECH_INPUT_API_CONSTANTS_H_

namespace extension_speech_input_api_constants {

extern const char kLanguageKey[];
extern const char kGrammarKey[];
extern const char kFilterProfanitiesKey[];

extern const char kErrorNoRecordingDeviceFound[];
extern const char kErrorRecordingDeviceInUse[];
extern const char kErrorUnableToStart[];
extern const char kErrorRequestDenied[];
extern const char kErrorRequestInProgress[];
extern const char kErrorInvalidOperation[];

extern const char kErrorCodeKey[];
extern const char kErrorCaptureError[];
extern const char kErrorNetworkError[];
extern const char kErrorNoSpeechHeard[];
extern const char kErrorNoResults[];

extern const char kUtteranceKey[];
extern const char kConfidenceKey[];
extern const char kHypothesesKey[];

extern const char kOnErrorEvent[];
extern const char kOnResultEvent[];
extern const char kOnSoundStartEvent[];
extern const char kOnSoundEndEvent[];

extern const char kDefaultGrammar[];
extern const bool kDefaultFilterProfanities;

}  // namespace extension_speech_input_api_constants.

#endif  // CHROME_BROWSER_EXTENSIONS_SPEECH_INPUT_EXTENSION_SPEECH_INPUT_API_CONSTANTS_H_
