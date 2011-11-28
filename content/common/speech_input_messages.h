// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard.

#include <string>

#include "content/public/common/speech_input_result.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "ui/gfx/rect.h"

#define IPC_MESSAGE_START SpeechInputMsgStart

IPC_ENUM_TRAITS(content::SpeechInputError)

IPC_STRUCT_TRAITS_BEGIN(content::SpeechInputHypothesis)
  IPC_STRUCT_TRAITS_MEMBER(utterance)
  IPC_STRUCT_TRAITS_MEMBER(confidence)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::SpeechInputResult)
  IPC_STRUCT_TRAITS_MEMBER(error)
  IPC_STRUCT_TRAITS_MEMBER(hypotheses)
IPC_STRUCT_TRAITS_END()

// Used to start a speech recognition session.
IPC_STRUCT_BEGIN(SpeechInputHostMsg_StartRecognition_Params)
  // The render view requesting speech recognition.
  IPC_STRUCT_MEMBER(int, render_view_id)
  // Request ID used within the render view.
  IPC_STRUCT_MEMBER(int, request_id)
  // Position of the UI element in page coordinates.
  IPC_STRUCT_MEMBER(gfx::Rect, element_rect)
  // Language to use for speech recognition.
  IPC_STRUCT_MEMBER(std::string, language)
  // Speech grammar given by the speech input element.
  IPC_STRUCT_MEMBER(std::string, grammar)
  // URL of the page (or iframe if applicable).
  IPC_STRUCT_MEMBER(std::string, origin_url)
IPC_STRUCT_END()

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
                    content::SpeechInputResult /* result */)

// Indicate that speech recognizer has stopped recording and started
// recognition.
IPC_MESSAGE_ROUTED1(SpeechInputMsg_RecordingComplete,
                    int /* request_id */)

// Indicate that speech recognizer has completed recognition. This will be
// the last message sent in response to a
// ViewHostMsg_SpeechInput_StartRecognition.
IPC_MESSAGE_ROUTED1(SpeechInputMsg_RecognitionComplete,
                    int /* request_id */)

// Toggle speech input on or off on the speech input control for the
// current focused element. Has no effect if the current element doesn't
// support speech input.
IPC_MESSAGE_ROUTED0(SpeechInputMsg_ToggleSpeechInput)
