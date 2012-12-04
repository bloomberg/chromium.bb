// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard.

#include <string>

#include "content/public/common/speech_recognition_error.h"
#include "content/public/common/speech_recognition_grammar.h"
#include "content/public/common/speech_recognition_result.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "ui/gfx/rect.h"

#define IPC_MESSAGE_START SpeechRecognitionMsgStart

IPC_ENUM_TRAITS(content::SpeechAudioErrorDetails)
IPC_ENUM_TRAITS(content::SpeechRecognitionErrorCode)

IPC_STRUCT_TRAITS_BEGIN(content::SpeechRecognitionError)
  IPC_STRUCT_TRAITS_MEMBER(code)
  IPC_STRUCT_TRAITS_MEMBER(details)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::SpeechRecognitionHypothesis)
  IPC_STRUCT_TRAITS_MEMBER(utterance)
  IPC_STRUCT_TRAITS_MEMBER(confidence)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::SpeechRecognitionResult)
  IPC_STRUCT_TRAITS_MEMBER(is_provisional)
  IPC_STRUCT_TRAITS_MEMBER(hypotheses)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::SpeechRecognitionGrammar)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(weight)
IPC_STRUCT_TRAITS_END()

// Used to start a speech recognition session.
IPC_STRUCT_BEGIN(InputTagSpeechHostMsg_StartRecognition_Params)
  // The render view requesting speech recognition.
  IPC_STRUCT_MEMBER(int, render_view_id)
  // Request ID used within the render view.
  IPC_STRUCT_MEMBER(int, request_id)
  // Position of the UI element in page coordinates.
  IPC_STRUCT_MEMBER(gfx::Rect, element_rect)
  // Language to use for speech recognition.
  IPC_STRUCT_MEMBER(std::string, language)
  // Speech grammar given by the speech recognition element.
  IPC_STRUCT_MEMBER(std::string, grammar)
  // URL of the page (or iframe if applicable).
  IPC_STRUCT_MEMBER(std::string, origin_url)
IPC_STRUCT_END()

// Renderer -> Browser messages.

// Requests the speech recognition service to start speech recognition on behalf
// of the given |render_view_id|.
IPC_MESSAGE_CONTROL1(InputTagSpeechHostMsg_StartRecognition,
                     InputTagSpeechHostMsg_StartRecognition_Params)

// Requests the speech recognition service to cancel speech recognition on
// behalf of the given |render_view_id|. If speech recognition is not happening
// or is happening on behalf of some other render view, this call does nothing.
IPC_MESSAGE_CONTROL2(InputTagSpeechHostMsg_CancelRecognition,
                     int /* render_view_id */,
                     int /* request_id */)

// Requests the speech recognition service to stop audio recording on behalf of
// the given |render_view_id|. Any audio recorded so far will be fed to the
// speech recognizer. If speech recognition is not happening nor or is
// happening on behalf of some other render view, this call does nothing.
IPC_MESSAGE_CONTROL2(InputTagSpeechHostMsg_StopRecording,
                     int /* render_view_id */,
                     int /* request_id */)

// Browser -> Renderer messages.

// Relays a speech recognition result, either partial or final.
IPC_MESSAGE_ROUTED2(InputTagSpeechMsg_SetRecognitionResults,
                    int /* request_id */,
                    content::SpeechRecognitionResults /* results */)

// Indicates that speech recognizer has stopped recording and started
// recognition.
IPC_MESSAGE_ROUTED1(InputTagSpeechMsg_RecordingComplete,
                    int /* request_id */)

// Indicates that speech recognizer has completed recognition. This will be the
// last message sent in response to a InputTagSpeechHostMsg_StartRecognition.
IPC_MESSAGE_ROUTED1(InputTagSpeechMsg_RecognitionComplete,
                    int /* request_id */)

// Toggles speech recognition on or off on the speech input control for the
// current focused element. Has no effect if the current element doesn't
// support speech recognition.
IPC_MESSAGE_ROUTED0(InputTagSpeechMsg_ToggleSpeechInput)


// ------- Messages for Speech JS APIs (SpeechRecognitionDispatcher) ----------

// Renderer -> Browser messages.

// Used to start a speech recognition session.
IPC_STRUCT_BEGIN(SpeechRecognitionHostMsg_StartRequest_Params)
  // The render view requesting speech recognition.
  IPC_STRUCT_MEMBER(int, render_view_id)
  // Unique ID associated with the JS object making the calls.
  IPC_STRUCT_MEMBER(int, request_id)
  // Language to use for speech recognition.
  IPC_STRUCT_MEMBER(std::string, language)
  // Speech grammars to use.
  IPC_STRUCT_MEMBER(content::SpeechRecognitionGrammarArray, grammars)
  // URL of the page (or iframe if applicable).
  IPC_STRUCT_MEMBER(std::string, origin_url)
  // Maximum number of hypotheses allowed for each results.
  IPC_STRUCT_MEMBER(uint32, max_hypotheses)
  // Whether the user requested continuous recognition or not.
  IPC_STRUCT_MEMBER(bool, continuous)
  // Whether the user requested interim results or not.
  IPC_STRUCT_MEMBER(bool, interim_results)
IPC_STRUCT_END()


// Requests the speech recognition service to start speech recognition.
IPC_MESSAGE_CONTROL1(SpeechRecognitionHostMsg_StartRequest,
                     SpeechRecognitionHostMsg_StartRequest_Params)

// Requests the speech recognition service to abort speech recognition on
// behalf of the given |render_view_id|. If speech recognition is not happening
// or is happening on behalf of some other render view, this call does nothing.
IPC_MESSAGE_CONTROL2(SpeechRecognitionHostMsg_AbortRequest,
                     int /* render_view_id */,
                     int /* request_id */)

// Requests the speech recognition service to stop audio capture on behalf of
// the given |render_view_id|. Any audio recorded so far will be fed to the
// speech recognizer. If speech recognition is not happening nor or is
// happening on behalf of some other render view, this call does nothing.
IPC_MESSAGE_CONTROL2(SpeechRecognitionHostMsg_StopCaptureRequest,
                     int /* render_view_id */,
                     int /* request_id */)

// Browser -> Renderer messages.

// The messages below follow exactly the same semantic of the corresponding
// events defined in content/public/browser/speech_recognition_event_listener.h.
IPC_MESSAGE_ROUTED2(SpeechRecognitionMsg_ResultRetrieved,
                    int /* request_id */,
                    content::SpeechRecognitionResults /* results */)

IPC_MESSAGE_ROUTED2(SpeechRecognitionMsg_ErrorOccurred,
                    int /* request_id */,
                    content::SpeechRecognitionError /* error */)

IPC_MESSAGE_ROUTED1(SpeechRecognitionMsg_Started, int /* request_id */)

IPC_MESSAGE_ROUTED1(SpeechRecognitionMsg_AudioStarted, int /* request_id */)

IPC_MESSAGE_ROUTED1(SpeechRecognitionMsg_SoundStarted, int /* request_id */)

IPC_MESSAGE_ROUTED1(SpeechRecognitionMsg_SoundEnded, int /* request_id */)

IPC_MESSAGE_ROUTED1(SpeechRecognitionMsg_AudioEnded, int /* request_id */)

IPC_MESSAGE_ROUTED1(SpeechRecognitionMsg_Ended, int /* request_id */)
