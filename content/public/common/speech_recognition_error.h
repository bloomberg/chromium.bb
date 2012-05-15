// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SPEECH_RECOGNITION_ERROR_H_
#define CONTENT_PUBLIC_COMMON_SPEECH_RECOGNITION_ERROR_H_
#pragma once

namespace content {

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

// Error details for the SPEECH_RECOGNITION_ERROR_AUDIO error.
enum SpeechAudioErrorDetails {
  SPEECH_AUDIO_ERROR_DETAILS_NONE = 0,
  SPEECH_AUDIO_ERROR_DETAILS_NO_MIC,
  SPEECH_AUDIO_ERROR_DETAILS_IN_USE
};

struct CONTENT_EXPORT SpeechRecognitionError {
  SpeechRecognitionErrorCode code;
  SpeechAudioErrorDetails details;

  SpeechRecognitionError()
      : code(SPEECH_RECOGNITION_ERROR_NONE),
        details(SPEECH_AUDIO_ERROR_DETAILS_NONE) {
  }
  explicit SpeechRecognitionError(SpeechRecognitionErrorCode code_value)
      : code(code_value),
        details(SPEECH_AUDIO_ERROR_DETAILS_NONE) {
  }
  SpeechRecognitionError(SpeechRecognitionErrorCode code_value,
                         SpeechAudioErrorDetails details_value)
      : code(code_value),
        details(details_value) {
  }
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SPEECH_RECOGNITION_ERROR_H_
