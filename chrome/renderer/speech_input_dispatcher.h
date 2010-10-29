// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SPEECH_INPUT_DISPATCHER_H_
#define CHROME_RENDERER_SPEECH_INPUT_DISPATCHER_H_

#include "base/basictypes.h"
#include "chrome/common/speech_input_result.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSpeechInputController.h"

class GURL;
class RenderView;

namespace WebKit {
class WebSpeechInputListener;
class WebString;
struct WebRect;
}

// SpeechInputDispatcher is a delegate for speech input messages used by WebKit.
// It's the complement of SpeechInputDispatcherHost (owned by RenderViewHost).
class SpeechInputDispatcher : public WebKit::WebSpeechInputController {
 public:
  SpeechInputDispatcher(RenderView* render_view,
                        WebKit::WebSpeechInputListener* listener);

  // Called to possibly handle the incoming IPC message. Returns true if
  // handled. Called in render thread.
  bool OnMessageReceived(const IPC::Message& msg);

  // WebKit::WebSpeechInputController.
  bool startRecognition(int request_id,
                        const WebKit::WebRect& element_rect,
                        const WebKit::WebString& language,
                        const WebKit::WebString& grammar);

  void cancelRecognition(int request_id);
  void stopRecording(int request_id);

 private:
  void OnSpeechRecognitionResult(
      int request_id, const speech_input::SpeechInputResultArray& result);
  void OnSpeechRecordingComplete(int request_id);
  void OnSpeechRecognitionComplete(int request_id);

  RenderView* render_view_;
  WebKit::WebSpeechInputListener* listener_;

  DISALLOW_COPY_AND_ASSIGN(SpeechInputDispatcher);
};

#endif  // CHROME_RENDERER_SPEECH_INPUT_DISPATCHER_H_
