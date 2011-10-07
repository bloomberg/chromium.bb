// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SPEECH_INPUT_RESULT_H_
#define CONTENT_COMMON_SPEECH_INPUT_RESULT_H_

#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"

namespace speech_input {

struct SpeechInputHypothesis {
  string16 utterance;
  double confidence;

  SpeechInputHypothesis() : confidence(0.0) {}

  SpeechInputHypothesis(const string16 utterance_value, double confidence_value)
      : utterance(utterance_value),
        confidence(confidence_value) {
  }
};

typedef std::vector<SpeechInputHypothesis> SpeechInputHypothesisArray;

// This enumeration follows the values described here:
// http://www.w3.org/2005/Incubator/htmlspeech/2010/10/google-api-draft.html#speech-input-error
enum SpeechInputError {
  kErrorNone = 0,   // There was no error.
  kErrorAborted,    // The user or a script aborted speech input.
  kErrorAudio,      // There was an error with recording audio.
  kErrorNetwork,    // There was a network error.
  kErrorNoSpeech,   // No speech heard before timeout.
  kErrorNoMatch,    // Speech was heard, but could not be interpreted.
  kErrorBadGrammar, // There was an error in the speech recognition grammar.
};

struct SpeechInputResult {
  SpeechInputError error;
  SpeechInputHypothesisArray hypotheses;

  SpeechInputResult();
  ~SpeechInputResult();
};

}  // namespace speech_input

#endif  // CONTENT_COMMON_SPEECH_INPUT_RESULT_H_
