// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/speech_recognition_dispatcher_host.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "content/common/speech_recognition_messages.h"
#include "content/public/browser/speech_recognition_manager.h"
#include "content/public/browser/speech_recognition_preferences.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "content/public/browser/speech_recognition_session_context.h"
#include "content/public/common/content_switches.h"

namespace content {
SpeechRecognitionManager* SpeechRecognitionDispatcherHost::manager_for_tests_;

void SpeechRecognitionDispatcherHost::SetManagerForTests(
    SpeechRecognitionManager* manager) {
  manager_for_tests_ = manager;
}

SpeechRecognitionDispatcherHost::SpeechRecognitionDispatcherHost(
    int render_process_id,
    net::URLRequestContextGetter* context_getter,
    SpeechRecognitionPreferences* recognition_preferences)
    : render_process_id_(render_process_id),
      context_getter_(context_getter),
      recognition_preferences_(recognition_preferences) {
  // Do not add any non-trivial initialization here, instead do it lazily when
  // required (e.g. see the method |manager()|) or add an Init() method.
}

SpeechRecognitionDispatcherHost::~SpeechRecognitionDispatcherHost() {
  if (SpeechRecognitionManager* sr_manager = manager())
    sr_manager->AbortAllSessionsForListener(this);
}

SpeechRecognitionManager* SpeechRecognitionDispatcherHost::manager() {
  if (manager_for_tests_)
    return manager_for_tests_;

  return SpeechRecognitionManager::GetInstance();
}

bool SpeechRecognitionDispatcherHost::OnMessageReceived(
    const IPC::Message& message, bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(SpeechRecognitionDispatcherHost, message,
                           *message_was_ok)
    IPC_MESSAGE_HANDLER(SpeechRecognitionHostMsg_StartRequest,
                        OnStartRequest)
    IPC_MESSAGE_HANDLER(SpeechRecognitionHostMsg_AbortRequest,
                        OnAbortRequest)
    IPC_MESSAGE_HANDLER(SpeechRecognitionHostMsg_StopCaptureRequest,
                        OnStopCaptureRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SpeechRecognitionDispatcherHost::OnStartRequest(
    const SpeechRecognitionHostMsg_StartRequest_Params& params) {

  SpeechRecognitionSessionContext context;
  context.context_name = params.origin_url;
  context.render_process_id = render_process_id_;
  context.render_view_id = params.render_view_id;
  context.request_id = params.request_id;
  context.requested_by_page_element = false;

  SpeechRecognitionSessionConfig config;
  config.is_legacy_api = false;
  config.language = params.language;
  config.grammars = params.grammars;
  config.max_hypotheses = params.max_hypotheses;
  config.origin_url = params.origin_url;
  config.initial_context = context;
  config.url_request_context_getter = context_getter_.get();
  if (recognition_preferences_) {
    config.filter_profanities = recognition_preferences_->FilterProfanities();
  } else {
    config.filter_profanities = false;
  }
  config.continuous = params.continuous;
  config.interim_results = params.interim_results;
  config.event_listener = this;

  int session_id = manager()->CreateSession(config);
  DCHECK_NE(session_id, SpeechRecognitionManager::kSessionIDInvalid);
  manager()->StartSession(session_id);
}

void SpeechRecognitionDispatcherHost::OnAbortRequest(int render_view_id,
                                                     int request_id) {
  int session_id = manager()->GetSession(render_process_id_,
                                         render_view_id,
                                         request_id);

  // The renderer might provide an invalid |request_id| if the session was not
  // started as expected, e.g., due to unsatisfied security requirements.
  if (session_id != SpeechRecognitionManager::kSessionIDInvalid)
    manager()->AbortSession(session_id);
}

void SpeechRecognitionDispatcherHost::OnStopCaptureRequest(
    int render_view_id, int request_id) {
  int session_id = manager()->GetSession(render_process_id_,
                                         render_view_id,
                                         request_id);

  // The renderer might provide an invalid |request_id| if the session was not
  // started as expected, e.g., due to unsatisfied security requirements.
  if (session_id != SpeechRecognitionManager::kSessionIDInvalid)
    manager()->StopAudioCaptureForSession(session_id);
}

// -------- SpeechRecognitionEventListener interface implementation -----------

void SpeechRecognitionDispatcherHost::OnRecognitionStart(int session_id) {
  const SpeechRecognitionSessionContext& context =
      manager()->GetSessionContext(session_id);
  Send(new SpeechRecognitionMsg_Started(context.render_view_id,
                                        context.request_id));
}

void SpeechRecognitionDispatcherHost::OnAudioStart(int session_id) {
  const SpeechRecognitionSessionContext& context =
      manager()->GetSessionContext(session_id);
  Send(new SpeechRecognitionMsg_AudioStarted(context.render_view_id,
                                             context.request_id));
}

void SpeechRecognitionDispatcherHost::OnSoundStart(int session_id) {
  const SpeechRecognitionSessionContext& context =
      manager()->GetSessionContext(session_id);
  Send(new SpeechRecognitionMsg_SoundStarted(context.render_view_id,
                                             context.request_id));
}

void SpeechRecognitionDispatcherHost::OnSoundEnd(int session_id) {
  const SpeechRecognitionSessionContext& context =
      manager()->GetSessionContext(session_id);
  Send(new SpeechRecognitionMsg_SoundEnded(context.render_view_id,
                                           context.request_id));
}

void SpeechRecognitionDispatcherHost::OnAudioEnd(int session_id) {
  const SpeechRecognitionSessionContext& context =
      manager()->GetSessionContext(session_id);
  Send(new SpeechRecognitionMsg_AudioEnded(context.render_view_id,
                                           context.request_id));
}

void SpeechRecognitionDispatcherHost::OnRecognitionEnd(int session_id) {
  const SpeechRecognitionSessionContext& context =
      manager()->GetSessionContext(session_id);
  Send(new SpeechRecognitionMsg_Ended(context.render_view_id,
                                      context.request_id));
}

void SpeechRecognitionDispatcherHost::OnRecognitionResults(
    int session_id,
    const SpeechRecognitionResults& results) {
  const SpeechRecognitionSessionContext& context =
      manager()->GetSessionContext(session_id);
  Send(new SpeechRecognitionMsg_ResultRetrieved(context.render_view_id,
                                                context.request_id,
                                                results));
}

void SpeechRecognitionDispatcherHost::OnRecognitionError(
    int session_id,
    const SpeechRecognitionError& error) {
  const SpeechRecognitionSessionContext& context =
      manager()->GetSessionContext(session_id);
  Send(new SpeechRecognitionMsg_ErrorOccurred(context.render_view_id,
                                              context.request_id,
                                              error));
}

// The events below are currently not used by speech JS APIs implementation.
void SpeechRecognitionDispatcherHost::OnAudioLevelsChange(int session_id,
                                                          float volume,
                                                          float noise_volume) {
}

void SpeechRecognitionDispatcherHost::OnEnvironmentEstimationComplete(
    int session_id) {
}

}  // namespace content
