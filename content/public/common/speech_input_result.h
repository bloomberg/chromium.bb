// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SPEECH_INPUT_RESULT_H_
#define CONTENT_PUBLIC_COMMON_SPEECH_INPUT_RESULT_H_

#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "content/common/content_export.h"

namespace content {

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
  // There was no error.
  SPEECH_INPUT_ERROR_NONE = 0,
  // The user or a script aborted speech input.
  SPEECH_INPUT_ERROR_ABORTED,
  // There was an error with recording audio.
  SPEECH_INPUT_ERROR_AUDIO,
  // There was a network error.
  SPEECH_INPUT_ERROR_NETWORK,
  // No speech heard before timeout.
  SPEECH_INPUT_ERROR_NO_SPEECH,
  // Speech was heard, but could not be interpreted.
  SPEECH_INPUT_ERROR_NO_MATCH,
  // There was an error in the speech recognition grammar.
  SPEECH_INPUT_ERROR_BAD_GRAMMAR,
};

struct CONTENT_EXPORT SpeechInputResult {
  SpeechInputError error;
  SpeechInputHypothesisArray hypotheses;

  SpeechInputResult();
  ~SpeechInputResult();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SPEECH_INPUT_RESULT_H_
