// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SPEECH_INPUT_DISPATCHER_H_
#define CHROME_RENDERER_SPEECH_INPUT_DISPATCHER_H_

#include "base/basictypes.h"
#include "chrome/common/speech_input_result.h"
#include "chrome/renderer/render_view_observer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSpeechInputController.h"

class GURL;

namespace WebKit {
class WebSpeechInputListener;
}

// SpeechInputDispatcher is a delegate for speech input messages used by WebKit.
// It's the complement of SpeechInputDispatcherHost (owned by RenderViewHost).
class SpeechInputDispatcher : public RenderViewObserver,
                              public WebKit::WebSpeechInputController {
 public:
  SpeechInputDispatcher(RenderView* render_view,
                        WebKit::WebSpeechInputListener* listener);

 private:
  // RenderView::Observer implementation.
  bool OnMessageReceived(const IPC::Message& message);

  // WebKit::WebSpeechInputController.
  virtual bool startRecognition(int request_id,
                                const WebKit::WebRect& element_rect,
                                const WebKit::WebString& language,
                                const WebKit::WebString& grammar,
                                const WebKit::WebSecurityOrigin& origin);

  virtual void cancelRecognition(int request_id);
  virtual void stopRecording(int request_id);

  void OnSpeechRecognitionResult(
      int request_id, const speech_input::SpeechInputResultArray& result);
  void OnSpeechRecordingComplete(int request_id);
  void OnSpeechRecognitionComplete(int request_id);

  WebKit::WebSpeechInputListener* listener_;

  DISALLOW_COPY_AND_ASSIGN(SpeechInputDispatcher);
};

#endif  // CHROME_RENDERER_SPEECH_INPUT_DISPATCHER_H_
