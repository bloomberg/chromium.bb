// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/speech_input/extension_speech_input_api_constants.h"

namespace extension_speech_input_api_constants {

const char kLanguageKey[] = "language";
const char kGrammarKey[] = "grammar";
const char kFilterProfanitiesKey[] = "filterProfanities";

const char kErrorNoRecordingDeviceFound[] = "noRecordingDeviceFound";
const char kErrorRecordingDeviceInUse[] = "recordingDeviceInUse";
const char kErrorUnableToStart[] = "unableToStart";
const char kErrorRequestDenied[] = "requestDenied";
const char kErrorRequestInProgress[] = "requestInProgress";
const char kErrorInvalidOperation[] = "invalidOperation";

const char kErrorCodeKey[] = "code";
const char kErrorCaptureError[] = "captureError";
const char kErrorNetworkError[] = "networkError";
const char kErrorNoSpeechHeard[] = "noSpeechHeard";
const char kErrorNoResults[] = "noResults";

const char kUtteranceKey[] = "utterance";
const char kConfidenceKey[] = "confidence";
const char kHypothesesKey[] = "hypotheses";

const char kOnErrorEvent[] = "experimental.speechInput.onError";
const char kOnResultEvent[] = "experimental.speechInput.onResult";
const char kOnSoundStartEvent[] = "experimental.speechInput.onSoundStart";
const char kOnSoundEndEvent[] = "experimental.speechInput.onSoundEnd";

const char kDefaultGrammar[] = "builtin:search";
const bool kDefaultFilterProfanities = true;

}  // namespace extension_speech_input_api_constants.
