// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_SESSION_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_SESSION_CONTEXT_H_
#pragma once

#include "content/common/content_export.h"
#include "ui/gfx/rect.h"

namespace content {

// The context information required by clients of the SpeechRecognitionManager
// (InputTagSpeechDispatcherHost) and its delegates for mapping the recognition
// session to other browser elements involved with the it (e.g., the page
// element that requested the recognition). The SpeechRecognitionManager is
// not aware of the content of this struct and does NOT use it for its purposes.
// However the manager keeps this struct "attached" to the recognition session
// during all the session lifetime, making its contents available to clients
// (In this regard, see SpeechRecognitionManager::GetSessionContext and
// SpeechRecognitionManager::LookupSessionByContext methods).
struct CONTENT_EXPORT SpeechRecognitionSessionContext {
  SpeechRecognitionSessionContext()
      : render_process_id(0),
        render_view_id(0),
        render_request_id(0) {}
  ~SpeechRecognitionSessionContext() {}

  int render_process_id;
  int render_view_id;
  int render_request_id;
  gfx::Rect element_rect;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_SESSION_CONTEXT_H_
