// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/speech_recognition_session_config.h"
#include "net/url_request/url_request_context_getter.h"

namespace content {

SpeechRecognitionSessionConfig::SpeechRecognitionSessionConfig()
    : is_one_shot(true),
      filter_profanities(false),
      event_listener(NULL) {
}

SpeechRecognitionSessionConfig::~SpeechRecognitionSessionConfig() {
}

}  // namespace content
