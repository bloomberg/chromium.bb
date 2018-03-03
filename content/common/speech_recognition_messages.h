// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SPEECH_RECOGNITION_MESSAGES_H_
#define CONTENT_COMMON_SPEECH_RECOGNITION_MESSAGES_H_

#include <stdint.h>

#include <string>

#include "content/public/common/speech_recognition_error.h"
#include "content/public/common/speech_recognition_grammar.h"
#include "content/public/common/speech_recognition_result.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "ui/gfx/geometry/rect.h"

#define IPC_MESSAGE_START SpeechRecognitionMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(content::SpeechAudioErrorDetails,
                          content::SPEECH_AUDIO_ERROR_DETAILS_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(content::SpeechRecognitionErrorCode,
                          content::SPEECH_RECOGNITION_ERROR_LAST)

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

// ------- Messages for Speech JS APIs (SpeechRecognitionDispatcher) ----------

// Renderer -> Browser messages.

// Used to start a speech recognition session.
IPC_STRUCT_BEGIN(SpeechRecognitionHostMsg_StartRequest_Params)
  // The render frame requesting speech recognition.
  IPC_STRUCT_MEMBER(int, render_frame_id)
  // Unique ID associated with the JS object making the calls.
  IPC_STRUCT_MEMBER(int, request_id)
  // Language to use for speech recognition.
  IPC_STRUCT_MEMBER(std::string, language)
  // Speech grammars to use.
  IPC_STRUCT_MEMBER(content::SpeechRecognitionGrammarArray, grammars)
  // URL of the page (or iframe if applicable).
  IPC_STRUCT_MEMBER(std::string, origin_url)
  // Maximum number of hypotheses allowed for each results.
  IPC_STRUCT_MEMBER(uint32_t, max_hypotheses)
  // Whether the user requested continuous recognition or not.
  IPC_STRUCT_MEMBER(bool, continuous)
  // Whether the user requested interim results or not.
  IPC_STRUCT_MEMBER(bool, interim_results)
IPC_STRUCT_END()


// Requests the speech recognition service to start speech recognition.
IPC_MESSAGE_CONTROL1(SpeechRecognitionHostMsg_StartRequest,
                     SpeechRecognitionHostMsg_StartRequest_Params)

// Requests the speech recognition service to abort speech recognition on
// behalf of the given |render_frame_id| and |request_id|. If there are no
// sessions associated with the |request_id| in the render frame, this call
// does nothing.
IPC_MESSAGE_CONTROL2(SpeechRecognitionHostMsg_AbortRequest,
                     int /* render_frame_id */,
                     int /* request_id */)

// Requests the speech recognition service to abort all speech recognitions on
// behalf of the given |render_frame_id|. If speech recognition is not happening
// or is happening on behalf of some other render frame, this call does nothing.
IPC_MESSAGE_CONTROL1(SpeechRecognitionHostMsg_AbortAllRequests,
                     int /* render_frame_id */)

// Requests the speech recognition service to stop audio capture on behalf of
// the given |render_frame_id|. Any audio recorded so far will be fed to the
// speech recognizer. If speech recognition is not happening nor or is
// happening on behalf of some other render frame, this call does nothing.
IPC_MESSAGE_CONTROL2(SpeechRecognitionHostMsg_StopCaptureRequest,
                     int /* render_frame_id */,
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

#endif  // CONTENT_COMMON_SPEECH_RECOGNITION_MESSAGES_H_
