// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_TAG_SPEECH_DISPATCHER_H_
#define CONTENT_RENDERER_INPUT_TAG_SPEECH_DISPATCHER_H_
#pragma once

#include "base/basictypes.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSpeechInputController.h"

class RenderViewImpl;

namespace content {
struct SpeechRecognitionResult;
}

namespace WebKit {
class WebSpeechInputListener;
}

// InputTagSpeechDispatcher is a delegate for messages used by WebKit. It's
// the complement of InputTagSpeechDispatcherHost (owned by RenderViewHost).
class InputTagSpeechDispatcher : public content::RenderViewObserver,
                                 public WebKit::WebSpeechInputController {
 public:
  InputTagSpeechDispatcher(RenderViewImpl* render_view,
                           WebKit::WebSpeechInputListener* listener);

 private:
  // RenderView::Observer implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // WebKit::WebSpeechInputController.
  virtual bool startRecognition(int request_id,
                                const WebKit::WebRect& element_rect,
                                const WebKit::WebString& language,
                                const WebKit::WebString& grammar,
                                const WebKit::WebSecurityOrigin& origin);

  virtual void cancelRecognition(int request_id);
  virtual void stopRecording(int request_id);

  void OnSpeechRecognitionResult(
      int request_id, const content::SpeechRecognitionResult& result);
  void OnSpeechRecordingComplete(int request_id);
  void OnSpeechRecognitionComplete(int request_id);
  void OnSpeechRecognitionToggleSpeechInput();

  WebKit::WebSpeechInputListener* listener_;

  DISALLOW_COPY_AND_ASSIGN(InputTagSpeechDispatcher);
};

#endif  // CONTENT_RENDERER_INPUT_TAG_SPEECH_DISPATCHER_H_
