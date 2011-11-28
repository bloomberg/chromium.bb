// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/speech_input_result.h"

namespace content {

SpeechInputResult::SpeechInputResult()
    : error(SPEECH_INPUT_ERROR_NONE) {
}

SpeechInputResult::~SpeechInputResult() {
}

}  // namespace content
