// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SPEECH_INPUT_RESULT_H_
#define CHROME_COMMON_SPEECH_INPUT_RESULT_H_

#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"

namespace speech_input {

struct SpeechInputResultItem {
  string16 utterance;
  double confidence;

  SpeechInputResultItem()
      : confidence(0.0) {
  }

  SpeechInputResultItem(const string16 utterance_value, double confidence_value)
      : utterance(utterance_value),
        confidence(confidence_value) {
  }
};

typedef std::vector<SpeechInputResultItem> SpeechInputResultArray;

}  // namespace speech_input

#endif  // CHROME_COMMON_SPEECH_INPUT_RESULT_H_
