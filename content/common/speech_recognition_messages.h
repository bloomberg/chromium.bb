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
