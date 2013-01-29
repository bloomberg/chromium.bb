// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_SESSION_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_SESSION_CONTEXT_H_

#include <string>

#include "content/common/content_export.h"
#include "content/public/common/media_stream_request.h"
#include "ui/gfx/rect.h"

namespace content {

// The context information required by clients of the SpeechRecognitionManager
// and its delegates for mapping the recognition session to other browser
// elements involved with it (e.g., the page element that requested the
// recognition). The manager keeps this struct attached to the recognition
// session during all the session lifetime, making its contents available to
// clients (In this regard, see SpeechRecognitionManager::GetSessionContext and
// SpeechRecognitionManager::LookupSessionByContext methods).
struct CONTENT_EXPORT SpeechRecognitionSessionContext {
  SpeechRecognitionSessionContext();
  ~SpeechRecognitionSessionContext();

  int render_process_id;
  int render_view_id;
  // Browser plugin guest's render view id, if this context represents a speech
  // recognition request from an embedder on behalf of the guest.
  int guest_render_view_id;
  int request_id;

  // Determines whether recognition was requested by a page element (in which
  // case its coordinates are passed in |element_rect|).
  bool requested_by_page_element;

  // The coordinates of the page element for placing the bubble (valid only when
  // |requested_by_page_element| = true).
  gfx::Rect element_rect;

  // A texual description of the context (website, extension name) that is
  // requesting recognition, for prompting security notifications to the user.
  std::string context_name;

  // The label for the permission request, it is used for request abortion.
  std::string label;

  // A list of devices being used by the recognition session.
  MediaStreamDevices devices;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_SESSION_CONTEXT_H_
