// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SPEECH_RECOGNITION_RESULT_H_
#define CONTENT_PUBLIC_COMMON_SPEECH_RECOGNITION_RESULT_H_

#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "content/common/content_export.h"

namespace content {

struct SpeechRecognitionHypothesis {
  string16 utterance;
  double confidence;

  SpeechRecognitionHypothesis() : confidence(0.0) {}

  SpeechRecognitionHypothesis(const string16 utterance_value,
                              double confidence_value)
      : utterance(utterance_value),
        confidence(confidence_value) {
  }
};

typedef std::vector<SpeechRecognitionHypothesis>
    SpeechRecognitionHypothesisArray;

// This enumeration follows the values described here:
// http://www.w3.org/2005/Incubator/htmlspeech/2010/10/google-api-draft.html#speech-input-error
enum SpeechRecognitionErrorCode {
  // There was no error.
  SPEECH_RECOGNITION_ERROR_NONE = 0,
  // The user or a script aborted speech input.
  SPEECH_RECOGNITION_ERROR_ABORTED,
  // There was an error with recording audio.
  SPEECH_RECOGNITION_ERROR_AUDIO,
  // There was a network error.
  SPEECH_RECOGNITION_ERROR_NETWORK,
  // No speech heard before timeout.
  SPEECH_RECOGNITION_ERROR_NO_SPEECH,
  // Speech was heard, but could not be interpreted.
  SPEECH_RECOGNITION_ERROR_NO_MATCH,
  // There was an error in the speech recognition grammar.
  SPEECH_RECOGNITION_ERROR_BAD_GRAMMAR,
};

struct CONTENT_EXPORT SpeechRecognitionResult {
  SpeechRecognitionErrorCode error;
  SpeechRecognitionHypothesisArray hypotheses;

  SpeechRecognitionResult();
  ~SpeechRecognitionResult();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SPEECH_RECOGNITION_RESULT_H_
