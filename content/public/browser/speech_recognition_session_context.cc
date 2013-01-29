// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/speech_recognition_session_context.h"

namespace content {

SpeechRecognitionSessionContext::SpeechRecognitionSessionContext()
    : render_process_id(0),
      render_view_id(0),
      guest_render_view_id(0),
      request_id(0),
      requested_by_page_element(true) {
}

SpeechRecognitionSessionContext::~SpeechRecognitionSessionContext() {
}

}  // namespace content
