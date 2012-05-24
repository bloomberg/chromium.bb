// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/input_tag_speech_dispatcher_host.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "content/browser/speech/speech_recognition_manager_impl.h"
#include "content/browser/speech/speech_recognizer_impl.h"
#include "content/common/speech_recognition_messages.h"
#include "content/public/browser/speech_recognition_preferences.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "content/public/browser/speech_recognition_session_context.h"

using content::BrowserThread;
using content::SpeechRecognitionManager;
using content::SpeechRecognitionSessionConfig;
using content::SpeechRecognitionSessionContext;

namespace {
bool IsSameContext(int render_process_id,
                   int render_view_id,
                   int render_request_id,
                   const SpeechRecognitionSessionContext& context) {
  return context.render_process_id == render_process_id &&
         context.render_view_id == render_view_id &&
         context.render_request_id == render_request_id;
}
}  // namespace

namespace speech {
SpeechRecognitionManager* InputTagSpeechDispatcherHost::manager_for_tests_;

void InputTagSpeechDispatcherHost::SetManagerForTests(
    SpeechRecognitionManager* manager) {
  manager_for_tests_ = manager;
}

InputTagSpeechDispatcherHost::InputTagSpeechDispatcherHost(
    int render_process_id,
    net::URLRequestContextGetter* url_request_context_getter,
    content::SpeechRecognitionPreferences* recognition_preferences)
    : render_process_id_(render_process_id),
      may_have_pending_requests_(false),
      url_request_context_getter_(url_request_context_getter),
      recognition_preferences_(recognition_preferences) {
  // This is initialized by Browser. Do not add any non-trivial
  // initialization here, instead do it lazily when required (e.g. see the
  // method |manager()|) or add an Init() method.
}

InputTagSpeechDispatcherHost::~InputTagSpeechDispatcherHost() {
  // If the renderer crashed for some reason or if we didn't receive a proper
  // Cancel/Stop call for an existing session, cancel such active sessions now.
  // We first check if this dispatcher received any speech IPC requst so that
  // we don't end up creating the speech input manager for web pages which don't
  // use speech input.
  if (may_have_pending_requests_)
    manager()->AbortAllSessionsForListener(this);
}

SpeechRecognitionManager* InputTagSpeechDispatcherHost::manager() {
  if (manager_for_tests_)
    return manager_for_tests_;
#if defined(ENABLE_INPUT_SPEECH)
  return SpeechRecognitionManager::GetInstance();
#else
  return NULL;
#endif
}

bool InputTagSpeechDispatcherHost::OnMessageReceived(
    const IPC::Message& message, bool* message_was_ok) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(InputTagSpeechDispatcherHost, message,
                           *message_was_ok)
    IPC_MESSAGE_HANDLER(InputTagSpeechHostMsg_StartRecognition,
                        OnStartRecognition)
    IPC_MESSAGE_HANDLER(InputTagSpeechHostMsg_CancelRecognition,
                        OnCancelRecognition)
    IPC_MESSAGE_HANDLER(InputTagSpeechHostMsg_StopRecording,
                        OnStopRecording)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  if (handled)
    may_have_pending_requests_ = true;
  return handled;
}

void InputTagSpeechDispatcherHost::OnStartRecognition(
    const InputTagSpeechHostMsg_StartRecognition_Params &params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  SpeechRecognitionSessionContext context;
  context.render_process_id = render_process_id_;
  context.render_view_id = params.render_view_id;
  context.render_request_id = params.request_id;
  context.element_rect = params.element_rect;

  SpeechRecognitionSessionConfig config;
  config.language = params.language;
  if (!params.grammar.empty()) {
    config.grammars.push_back(
        content::SpeechRecognitionGrammar(params.grammar));
  }
  config.origin_url = params.origin_url;
  config.initial_context = context;
  config.url_request_context_getter = url_request_context_getter_.get();
  config.filter_profanities = recognition_preferences_->FilterProfanities();
  config.event_listener = this;

  int session_id = manager()->CreateSession(config);
  if (session_id == SpeechRecognitionManager::kSessionIDInvalid)
    return;

   manager()->StartSession(session_id);
}

void InputTagSpeechDispatcherHost::OnCancelRecognition(int render_view_id,
                                                       int request_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  int session_id = manager()->LookupSessionByContext(
      base::Bind(&IsSameContext,
                 render_process_id_,
                 render_view_id,
                 request_id));
  if (session_id != SpeechRecognitionManager::kSessionIDInvalid)
    manager()->AbortSession(session_id);
}

void InputTagSpeechDispatcherHost::OnStopRecording(int render_view_id,
                                                   int request_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  int session_id = manager()->LookupSessionByContext(
      base::Bind(&IsSameContext,
                 render_process_id_,
                 render_view_id,
                 request_id));
  DCHECK_NE(session_id, SpeechRecognitionManager::kSessionIDInvalid);
  manager()->StopAudioCaptureForSession(session_id);
}

// -------- SpeechRecognitionEventListener interface implementation -----------
void InputTagSpeechDispatcherHost::OnRecognitionResult(
      int session_id, const content::SpeechRecognitionResult& result) {
  VLOG(1) << "InputTagSpeechDispatcherHost::OnRecognitionResult enter";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  const SpeechRecognitionSessionContext& context =
      manager()->GetSessionContext(session_id);

  Send(new InputTagSpeechMsg_SetRecognitionResult(
      context.render_view_id,
      context.render_request_id,
      result));
  VLOG(1) << "InputTagSpeechDispatcherHost::OnRecognitionResult exit";
}

void InputTagSpeechDispatcherHost::OnAudioEnd(int session_id) {
  VLOG(1) << "InputTagSpeechDispatcherHost::OnAudioEnd enter";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  const SpeechRecognitionSessionContext& context =
      manager()->GetSessionContext(session_id);

  Send(new InputTagSpeechMsg_RecordingComplete(context.render_view_id,
                                               context.render_request_id));
  VLOG(1) << "InputTagSpeechDispatcherHost::OnAudioEnd exit";
}

void InputTagSpeechDispatcherHost::OnRecognitionEnd(int session_id) {
  VLOG(1) << "InputTagSpeechDispatcherHost::OnRecognitionEnd enter";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  const SpeechRecognitionSessionContext& context =
      manager()->GetSessionContext(session_id);
  Send(new InputTagSpeechMsg_RecognitionComplete(context.render_view_id,
                                                 context.render_request_id));
  VLOG(1) << "InputTagSpeechDispatcherHost::OnRecognitionEnd exit";
}

// The events below are currently not used by x-webkit-speech implementation.
void InputTagSpeechDispatcherHost::OnRecognitionStart(int session_id) {}
void InputTagSpeechDispatcherHost::OnAudioStart(int session_id) {}
void InputTagSpeechDispatcherHost::OnSoundStart(int session_id) {}
void InputTagSpeechDispatcherHost::OnSoundEnd(int session_id) {}
void InputTagSpeechDispatcherHost::OnRecognitionError(
    int session_id, const content::SpeechRecognitionError& error) {}
void InputTagSpeechDispatcherHost::OnAudioLevelsChange(
    int session_id, float volume, float noise_volume) {}
void InputTagSpeechDispatcherHost::OnEnvironmentEstimationComplete(
    int session_id) {}

}  // namespace speech
