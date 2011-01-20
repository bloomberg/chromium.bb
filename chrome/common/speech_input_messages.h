// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SPEECH_INPUT_MESSAGES_H_
#define CHROME_COMMON_SPEECH_INPUT_MESSAGES_H_
#pragma once

#include "chrome/common/speech_input_result.h"
#include "gfx/rect.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"

#define IPC_MESSAGE_START SpeechInputMsgStart

namespace speech_input {
struct SpeechInputResultItem;
}

// Used to start a speech recognition session.
struct SpeechInputHostMsg_StartRecognition_Params {
  SpeechInputHostMsg_StartRecognition_Params();
  ~SpeechInputHostMsg_StartRecognition_Params();

  int render_view_id;  // The render view requesting speech recognition.
  int request_id;  // Request ID used within the render view.
  gfx::Rect element_rect;  // Position of the UI element in page coordinates.
  std::string language;  // Language to use for speech recognition.
  std::string grammar;  // Speech grammar given by the speech input element.
  std::string origin_url;  // URL of the page (or iframe if applicable).
};

namespace IPC {

template <>
struct ParamTraits<speech_input::SpeechInputResultItem> {
  typedef speech_input::SpeechInputResultItem param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<SpeechInputHostMsg_StartRecognition_Params> {
  typedef SpeechInputHostMsg_StartRecognition_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

// Speech input messages sent from the renderer to the browser.

// Requests the speech input service to start speech recognition on behalf of
// the given |render_view_id|.
IPC_MESSAGE_CONTROL1(SpeechInputHostMsg_StartRecognition,
                     SpeechInputHostMsg_StartRecognition_Params)

// Requests the speech input service to cancel speech recognition on behalf of
// the given |render_view_id|. If speech recognition is not happening or
// is happening on behalf of some other render view, this call does nothing.
IPC_MESSAGE_CONTROL2(SpeechInputHostMsg_CancelRecognition,
                     int /* render_view_id */,
                     int /* request_id */)

// Requests the speech input service to stop audio recording on behalf of
// the given |render_view_id|. Any audio recorded so far will be fed to the
// speech recognizer. If speech recognition is not happening nor or is
// happening on behalf of some other render view, this call does nothing.
IPC_MESSAGE_CONTROL2(SpeechInputHostMsg_StopRecording,
                     int /* render_view_id */,
                     int /* request_id */)

// Speech input messages sent from the browser to the renderer.

// Relay a speech recognition result, either partial or final.
IPC_MESSAGE_ROUTED2(SpeechInputMsg_SetRecognitionResult,
                    int /* request_id */,
                    speech_input::SpeechInputResultArray /* result */)

// Indicate that speech recognizer has stopped recording and started
// recognition.
IPC_MESSAGE_ROUTED1(SpeechInputMsg_RecordingComplete,
                    int /* request_id */)

// Indicate that speech recognizer has completed recognition. This will be
// the last message sent in response to a
// ViewHostMsg_SpeechInput_StartRecognition.
IPC_MESSAGE_ROUTED1(SpeechInputMsg_RecognitionComplete,
                    int /* request_id */)

#endif  // CHROME_COMMON_SPEECH_INPUT_MESSAGES_H_
