// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/speech_recognition_dispatcher.h"

#include "base/basictypes.h"
#include "base/utf_string_conversions.h"
#include "content/common/speech_recognition_messages.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSpeechGrammar.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSpeechRecognitionParams.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSpeechRecognitionResult.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSpeechRecognizerClient.h"

using content::SpeechRecognitionError;
using content::SpeechRecognitionResult;
using WebKit::WebVector;
using WebKit::WebString;
using WebKit::WebSpeechGrammar;
using WebKit::WebSpeechRecognitionHandle;
using WebKit::WebSpeechRecognitionResult;
using WebKit::WebSpeechRecognitionParams;
using WebKit::WebSpeechRecognizerClient;

SpeechRecognitionDispatcher::SpeechRecognitionDispatcher(
    RenderViewImpl* render_view)
    : content::RenderViewObserver(render_view),
      recognizer_client_(NULL),
      next_id_(1) {
}

SpeechRecognitionDispatcher::~SpeechRecognitionDispatcher() {
}

bool SpeechRecognitionDispatcher::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SpeechRecognitionDispatcher, message)
    IPC_MESSAGE_HANDLER(SpeechRecognitionMsg_Started, OnRecognitionStarted)
    IPC_MESSAGE_HANDLER(SpeechRecognitionMsg_AudioStarted, OnAudioStarted)
    IPC_MESSAGE_HANDLER(SpeechRecognitionMsg_SoundStarted, OnSoundStarted)
    IPC_MESSAGE_HANDLER(SpeechRecognitionMsg_SoundEnded, OnSoundEnded)
    IPC_MESSAGE_HANDLER(SpeechRecognitionMsg_AudioEnded, OnAudioEnded)
    IPC_MESSAGE_HANDLER(SpeechRecognitionMsg_ErrorOccurred, OnErrorOccurred)
    IPC_MESSAGE_HANDLER(SpeechRecognitionMsg_Ended, OnRecognitionEnded)
    IPC_MESSAGE_HANDLER(SpeechRecognitionMsg_ResultRetrieved, OnResultRetrieved)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SpeechRecognitionDispatcher::start(
    const WebSpeechRecognitionHandle& handle,
    const WebSpeechRecognitionParams& params,
    WebSpeechRecognizerClient* recognizer_client) {
  DCHECK(!recognizer_client_ || recognizer_client_ == recognizer_client);
  recognizer_client_ = recognizer_client;

  SpeechRecognitionHostMsg_StartRequest_Params msg_params;
  for (size_t i = 0; i < params.grammars().size(); ++i) {
    const WebSpeechGrammar& grammar = params.grammars()[i];
    msg_params.grammars.push_back(
        content::SpeechRecognitionGrammar(grammar.src().spec(),
                                          grammar.weight()));
  }
  msg_params.language = UTF16ToUTF8(params.language());
  msg_params.is_one_shot = !params.continuous();
  msg_params.max_hypotheses = static_cast<uint32>(params.maxAlternatives());
  msg_params.origin_url = params.origin().toString().utf8();
  msg_params.render_view_id = routing_id();
  msg_params.request_id = GetOrCreateIDForHandle(handle);
  // The handle mapping will be removed in |OnRecognitionEnd|.
  Send(new SpeechRecognitionHostMsg_StartRequest(msg_params));
}

void SpeechRecognitionDispatcher::stop(
    const WebSpeechRecognitionHandle& handle,
    WebSpeechRecognizerClient* recognizer_client) {
  // Ignore a |stop| issued without a matching |start|.
  if (recognizer_client_ != recognizer_client || !HandleExists(handle))
    return;
  Send(new SpeechRecognitionHostMsg_StopCaptureRequest(
      routing_id(), GetOrCreateIDForHandle(handle)));
}

void SpeechRecognitionDispatcher::abort(
    const WebSpeechRecognitionHandle& handle,
    WebSpeechRecognizerClient* recognizer_client) {
  // Ignore an |abort| issued without a matching |start|.
  if (recognizer_client_ != recognizer_client || !HandleExists(handle))
    return;
  Send(new SpeechRecognitionHostMsg_AbortRequest(
      routing_id(), GetOrCreateIDForHandle(handle)));
}

void SpeechRecognitionDispatcher::OnRecognitionStarted(int request_id) {
  recognizer_client_->didStart(GetHandleFromID(request_id));
}

void SpeechRecognitionDispatcher::OnAudioStarted(int request_id) {
  recognizer_client_->didStartAudio(GetHandleFromID(request_id));
}

void SpeechRecognitionDispatcher::OnSoundStarted(int request_id) {
  recognizer_client_->didStartSound(GetHandleFromID(request_id));
}

void SpeechRecognitionDispatcher::OnSoundEnded(int request_id) {
  recognizer_client_->didEndSound(GetHandleFromID(request_id));
}

void SpeechRecognitionDispatcher::OnAudioEnded(int request_id) {
  recognizer_client_->didEndAudio(GetHandleFromID(request_id));
}

static WebSpeechRecognizerClient::ErrorCode WebKitErrorCode(
    content::SpeechRecognitionErrorCode e) {
  switch (e) {
    case content::SPEECH_RECOGNITION_ERROR_NONE:
      NOTREACHED();
      return WebSpeechRecognizerClient::OtherError;
    case content::SPEECH_RECOGNITION_ERROR_ABORTED:
      return WebSpeechRecognizerClient::AbortedError;
    case content::SPEECH_RECOGNITION_ERROR_AUDIO:
      return WebSpeechRecognizerClient::AudioCaptureError;
    case content::SPEECH_RECOGNITION_ERROR_NETWORK:
      return WebSpeechRecognizerClient::NetworkError;
    case content::SPEECH_RECOGNITION_ERROR_NOT_ALLOWED:
      return WebSpeechRecognizerClient::NotAllowedError;
    case content::SPEECH_RECOGNITION_ERROR_NO_SPEECH:
      return WebSpeechRecognizerClient::NoSpeechError;
    case content::SPEECH_RECOGNITION_ERROR_NO_MATCH:
      NOTREACHED();
      return WebSpeechRecognizerClient::OtherError;
    case content::SPEECH_RECOGNITION_ERROR_BAD_GRAMMAR:
      return WebSpeechRecognizerClient::BadGrammarError;
  }
  NOTREACHED();
  return WebSpeechRecognizerClient::OtherError;
}

void SpeechRecognitionDispatcher::OnErrorOccurred(
    int request_id, const SpeechRecognitionError& error) {
  if (error.code == content::SPEECH_RECOGNITION_ERROR_NO_MATCH) {
    recognizer_client_->didReceiveNoMatch(GetHandleFromID(request_id),
                                          WebSpeechRecognitionResult());
  } else {
    recognizer_client_->didReceiveError(GetHandleFromID(request_id),
                                        WebString(), // TODO(primiano): message?
                                        WebKitErrorCode(error.code));
  }
}

void SpeechRecognitionDispatcher::OnRecognitionEnded(int request_id) {
  recognizer_client_->didEnd(GetHandleFromID(request_id));
  handle_map_.erase(request_id);
}

void SpeechRecognitionDispatcher::OnResultRetrieved(
    int request_id, const SpeechRecognitionResult& result) {
  const size_t num_hypotheses = result.hypotheses.size();
  WebSpeechRecognitionResult webkit_result;
  WebVector<WebString> transcripts(num_hypotheses);
  WebVector<float> confidences(num_hypotheses);
  for (size_t i = 0; i < num_hypotheses; ++i) {
    transcripts[i] = result.hypotheses[i].utterance;
    confidences[i] = static_cast<float>(result.hypotheses[i].confidence);
  }
  webkit_result.assign(transcripts, confidences, !result.is_provisional);
  // TODO(primiano): Handle history, currently empty.
  WebVector<WebSpeechRecognitionResult> empty_history;
  recognizer_client_->didReceiveResult(GetHandleFromID(request_id),
                                       webkit_result,
                                       0, // result_index
                                       empty_history);
}


int SpeechRecognitionDispatcher::GetOrCreateIDForHandle(
    const WebSpeechRecognitionHandle& handle) {
  // Search first for an existing mapping.
  for (HandleMap::iterator iter = handle_map_.begin();
      iter != handle_map_.end();
      ++iter) {
    if (iter->second.equals(handle))
      return iter->first;
  }
  // If no existing mapping found, create a new one.
  const int new_id = next_id_;
  handle_map_[new_id] = handle;
  ++next_id_;
  return new_id;
}

bool SpeechRecognitionDispatcher::HandleExists(
    const WebSpeechRecognitionHandle& handle) {
  for (HandleMap::iterator iter = handle_map_.begin();
      iter != handle_map_.end();
      ++iter) {
    if (iter->second.equals(handle))
      return true;
  }
  return false;
}

const WebSpeechRecognitionHandle& SpeechRecognitionDispatcher::GetHandleFromID(
    int request_id) {
  HandleMap::iterator iter = handle_map_.find(request_id);
  DCHECK(iter != handle_map_.end());
  return iter->second;
}
