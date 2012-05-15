// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SPEECH_RECOGNITION_RESULT_H_
#define CONTENT_PUBLIC_COMMON_SPEECH_RECOGNITION_RESULT_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "content/common/content_export.h"

namespace content {

struct SpeechRecognitionHypothesis {
  string16 utterance;
  double confidence;

  SpeechRecognitionHypothesis() : confidence(0.0) {}

  SpeechRecognitionHypothesis(const string16& utterance_value,
                              double confidence_value)
      : utterance(utterance_value),
        confidence(confidence_value) {
  }
};

typedef std::vector<SpeechRecognitionHypothesis>
    SpeechRecognitionHypothesisArray;

struct CONTENT_EXPORT SpeechRecognitionResult {
  SpeechRecognitionHypothesisArray hypotheses;
  bool provisional;

  SpeechRecognitionResult();
  ~SpeechRecognitionResult();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SPEECH_RECOGNITION_RESULT_H_
