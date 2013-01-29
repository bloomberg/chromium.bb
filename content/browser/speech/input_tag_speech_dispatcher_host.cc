// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/input_tag_speech_dispatcher_host.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/speech_recognition_messages.h"
#include "content/public/browser/speech_recognition_manager.h"
#include "content/public/browser/speech_recognition_preferences.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "content/public/browser/speech_recognition_session_context.h"

namespace {
const uint32 kMaxHypothesesForSpeechInputTag = 6;
}

namespace content {

InputTagSpeechDispatcherHost::InputTagSpeechDispatcherHost(
    bool guest,
    int render_process_id,
    net::URLRequestContextGetter* url_request_context_getter,
    SpeechRecognitionPreferences* recognition_preferences)
    : guest_(guest),
      render_process_id_(render_process_id),
      url_request_context_getter_(url_request_context_getter),
      recognition_preferences_(recognition_preferences) {
  // Do not add any non-trivial initialization here, instead do it lazily when
  // required (e.g. see the method |manager()|) or add an Init() method.
}

InputTagSpeechDispatcherHost::~InputTagSpeechDispatcherHost() {
  if (SpeechRecognitionManager* sr_manager = manager())
    sr_manager->AbortAllSessionsForListener(this);
}

SpeechRecognitionManager* InputTagSpeechDispatcherHost::manager() {
  return SpeechRecognitionManager::GetInstance();
}

bool InputTagSpeechDispatcherHost::OnMessageReceived(
    const IPC::Message& message, bool* message_was_ok) {
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
  return handled;
}

void InputTagSpeechDispatcherHost::OnStartRecognition(
    const InputTagSpeechHostMsg_StartRecognition_Params& params) {
  if (guest_ && !BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            &InputTagSpeechDispatcherHost::OnStartRecognition,
            this,
            params));
    return;
  }

  DCHECK(!guest_ || BrowserThread::CurrentlyOn(BrowserThread::UI));

  InputTagSpeechHostMsg_StartRecognition_Params input_params(params);
  int render_process_id = render_process_id_;
  // The chrome layer is mostly oblivious to BrowserPlugin guests and so it
  // cannot correctly place the speech bubble relative to a guest. Thus, we
  // set up the speech recognition context relative to the embedder.
  int guest_render_view_id = 0;
  if (guest_) {
    RenderViewHostImpl* render_view_host =
        RenderViewHostImpl::FromID(render_process_id_, params.render_view_id);
    WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
        WebContents::FromRenderViewHost(render_view_host));
    BrowserPluginGuest* guest = web_contents->GetBrowserPluginGuest();
    input_params.element_rect.set_origin(
        guest->GetScreenCoordinates(input_params.element_rect.origin()));
    guest_render_view_id = params.render_view_id;
    render_process_id =
        guest->embedder_web_contents()->GetRenderProcessHost()->GetID();
    input_params.render_view_id =
        guest->embedder_web_contents()->GetRoutingID();
  }

  if (guest_) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(
            &InputTagSpeechDispatcherHost::StartRecognitionOnIO,
            this,
            render_process_id,
            guest_render_view_id,
            input_params));
  } else {
    StartRecognitionOnIO(render_process_id, guest_render_view_id, params);
  }
}

void InputTagSpeechDispatcherHost::StartRecognitionOnIO(
    int render_process_id,
    int guest_render_view_id,
    const InputTagSpeechHostMsg_StartRecognition_Params& params) {
  SpeechRecognitionSessionContext context;
  context.render_process_id = render_process_id;
  context.render_view_id = params.render_view_id;
  context.guest_render_view_id = guest_render_view_id;
  context.request_id = params.request_id;
  context.element_rect = params.element_rect;

  SpeechRecognitionSessionConfig config;
  config.language = params.language;
  if (!params.grammar.empty()) {
    config.grammars.push_back(SpeechRecognitionGrammar(params.grammar));
  }
  config.max_hypotheses = kMaxHypothesesForSpeechInputTag;
  config.origin_url = params.origin_url;
  config.initial_context = context;
  config.url_request_context_getter = url_request_context_getter_.get();
  if (recognition_preferences_) {
    config.filter_profanities = recognition_preferences_->FilterProfanities();
  } else {
    config.filter_profanities = false;
  }
  config.event_listener = this;

  int session_id = manager()->CreateSession(config);
  DCHECK_NE(session_id, SpeechRecognitionManager::kSessionIDInvalid);
  manager()->StartSession(session_id);
}

void InputTagSpeechDispatcherHost::OnCancelRecognition(int render_view_id,
                                                       int request_id) {
  int session_id = manager()->GetSession(render_process_id_,
                                         render_view_id,
                                         request_id);

  // The renderer might provide an invalid |request_id| if the session was not
  // started as expected, e.g., due to unsatisfied security requirements.
  if (session_id != SpeechRecognitionManager::kSessionIDInvalid)
    manager()->AbortSession(session_id);
}

void InputTagSpeechDispatcherHost::OnStopRecording(int render_view_id,
                                                   int request_id) {
  int session_id = manager()->GetSession(render_process_id_,
                                         render_view_id,
                                         request_id);

  // The renderer might provide an invalid |request_id| if the session was not
  // started as expected, e.g., due to unsatisfied security requirements.
  if (session_id != SpeechRecognitionManager::kSessionIDInvalid)
    manager()->StopAudioCaptureForSession(session_id);
}

// -------- SpeechRecognitionEventListener interface implementation -----------
void InputTagSpeechDispatcherHost::OnRecognitionResults(
    int session_id,
    const SpeechRecognitionResults& results) {
  DVLOG(1) << "InputTagSpeechDispatcherHost::OnRecognitionResults enter";

  const SpeechRecognitionSessionContext& context =
      manager()->GetSessionContext(session_id);

  int render_view_id = context.guest_render_view_id ?
      context.guest_render_view_id : context.render_view_id;
  Send(new InputTagSpeechMsg_SetRecognitionResults(
      render_view_id,
      context.request_id,
      results));
  DVLOG(1) << "InputTagSpeechDispatcherHost::OnRecognitionResults exit";
}

void InputTagSpeechDispatcherHost::OnAudioEnd(int session_id) {
  DVLOG(1) << "InputTagSpeechDispatcherHost::OnAudioEnd enter";

  const SpeechRecognitionSessionContext& context =
      manager()->GetSessionContext(session_id);
  int render_view_id = context.guest_render_view_id ?
      context.guest_render_view_id : context.render_view_id;
  Send(new InputTagSpeechMsg_RecordingComplete(render_view_id,
                                               context.request_id));
  DVLOG(1) << "InputTagSpeechDispatcherHost::OnAudioEnd exit";
}

void InputTagSpeechDispatcherHost::OnRecognitionEnd(int session_id) {
  DVLOG(1) << "InputTagSpeechDispatcherHost::OnRecognitionEnd enter";
  const SpeechRecognitionSessionContext& context =
      manager()->GetSessionContext(session_id);
  int render_view_id = context.guest_render_view_id ?
      context.guest_render_view_id : context.render_view_id;
  Send(new InputTagSpeechMsg_RecognitionComplete(render_view_id,
                                                 context.request_id));
  DVLOG(1) << "InputTagSpeechDispatcherHost::OnRecognitionEnd exit";
}

// The events below are currently not used by x-webkit-speech implementation.
void InputTagSpeechDispatcherHost::OnRecognitionStart(int session_id) {}
void InputTagSpeechDispatcherHost::OnAudioStart(int session_id) {}
void InputTagSpeechDispatcherHost::OnSoundStart(int session_id) {}
void InputTagSpeechDispatcherHost::OnSoundEnd(int session_id) {}
void InputTagSpeechDispatcherHost::OnRecognitionError(
    int session_id,
    const SpeechRecognitionError& error) {}
void InputTagSpeechDispatcherHost::OnAudioLevelsChange(
    int session_id, float volume, float noise_volume) {}
void InputTagSpeechDispatcherHost::OnEnvironmentEstimationComplete(
    int session_id) {}

}  // namespace content
