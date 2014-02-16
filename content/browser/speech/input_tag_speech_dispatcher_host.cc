// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/input_tag_speech_dispatcher_host.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/speech/speech_recognition_manager_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/speech_recognition_messages.h"
#include "content/public/browser/speech_recognition_manager_delegate.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "content/public/browser/speech_recognition_session_context.h"

namespace {
const uint32 kMaxHypothesesForSpeechInputTag = 6;
}

namespace content {

InputTagSpeechDispatcherHost::InputTagSpeechDispatcherHost(
    bool is_guest,
    int render_process_id,
    net::URLRequestContextGetter* url_request_context_getter)
    : BrowserMessageFilter(SpeechRecognitionMsgStart),
      is_guest_(is_guest),
      render_process_id_(render_process_id),
      url_request_context_getter_(url_request_context_getter) {
  // Do not add any non-trivial initialization here, instead do it lazily when
  // required (e.g. see the method |SpeechRecognitionManager::GetInstance()|) or
  // add an Init() method.
}

InputTagSpeechDispatcherHost::~InputTagSpeechDispatcherHost() {
  SpeechRecognitionManager::GetInstance()->AbortAllSessionsForListener(this);
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

void InputTagSpeechDispatcherHost::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  if (message.type() == InputTagSpeechHostMsg_StartRecognition::ID)
    *thread = BrowserThread::UI;
}

void InputTagSpeechDispatcherHost::OnStartRecognition(
    const InputTagSpeechHostMsg_StartRecognition_Params& params) {  
  InputTagSpeechHostMsg_StartRecognition_Params input_params(params);
  int render_process_id = render_process_id_;
  // The chrome layer is mostly oblivious to BrowserPlugin guests and so it
  // cannot correctly place the speech bubble relative to a guest. Thus, we
  // set up the speech recognition context relative to the embedder.
  int guest_render_view_id = MSG_ROUTING_NONE;
  if (is_guest_) {
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
  bool filter_profanities =
      SpeechRecognitionManagerImpl::GetInstance() &&
      SpeechRecognitionManagerImpl::GetInstance()->delegate() &&
      SpeechRecognitionManagerImpl::GetInstance()->delegate()->
          FilterProfanities(render_process_id_);

 BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &InputTagSpeechDispatcherHost::StartRecognitionOnIO,
          this,
          render_process_id,
          guest_render_view_id,
          input_params,
          filter_profanities));
}

void InputTagSpeechDispatcherHost::StartRecognitionOnIO(
    int render_process_id,
    int guest_render_view_id,
    const InputTagSpeechHostMsg_StartRecognition_Params& params,
    bool filter_profanities) {
  SpeechRecognitionSessionContext context;
  context.render_process_id = render_process_id;
  context.render_view_id = params.render_view_id;
  context.guest_render_view_id = guest_render_view_id;
  // Keep context.embedder_render_process_id and context.embedder_render_view_id
  // unset.
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
  config.filter_profanities = filter_profanities;
  config.event_listener = this;

  int session_id = SpeechRecognitionManager::GetInstance()->CreateSession(
      config);
  DCHECK_NE(session_id, SpeechRecognitionManager::kSessionIDInvalid);
  SpeechRecognitionManager::GetInstance()->StartSession(session_id);
}

void InputTagSpeechDispatcherHost::OnCancelRecognition(int render_view_id,
                                                       int request_id) {
  int session_id = SpeechRecognitionManager::GetInstance()->GetSession(
      render_process_id_, render_view_id, request_id);

  // The renderer might provide an invalid |request_id| if the session was not
  // started as expected, e.g., due to unsatisfied security requirements.
  if (session_id != SpeechRecognitionManager::kSessionIDInvalid)
    SpeechRecognitionManager::GetInstance()->AbortSession(session_id);
}

void InputTagSpeechDispatcherHost::OnStopRecording(int render_view_id,
                                                   int request_id) {
  int session_id = SpeechRecognitionManager::GetInstance()->GetSession(
      render_process_id_, render_view_id, request_id);

  // The renderer might provide an invalid |request_id| if the session was not
  // started as expected, e.g., due to unsatisfied security requirements.
  if (session_id != SpeechRecognitionManager::kSessionIDInvalid) {
    SpeechRecognitionManager::GetInstance()->StopAudioCaptureForSession(
        session_id);
  }
}

// -------- SpeechRecognitionEventListener interface implementation -----------
void InputTagSpeechDispatcherHost::OnRecognitionResults(
    int session_id,
    const SpeechRecognitionResults& results) {
  DVLOG(1) << "InputTagSpeechDispatcherHost::OnRecognitionResults enter";

  const SpeechRecognitionSessionContext& context =
      SpeechRecognitionManager::GetInstance()->GetSessionContext(session_id);

  int render_view_id =
      context.guest_render_view_id == MSG_ROUTING_NONE ?
          context.render_view_id : context.guest_render_view_id;
  Send(new InputTagSpeechMsg_SetRecognitionResults(
      render_view_id,
      context.request_id,
      results));
  DVLOG(1) << "InputTagSpeechDispatcherHost::OnRecognitionResults exit";
}

void InputTagSpeechDispatcherHost::OnAudioEnd(int session_id) {
  DVLOG(1) << "InputTagSpeechDispatcherHost::OnAudioEnd enter";

  const SpeechRecognitionSessionContext& context =
      SpeechRecognitionManager::GetInstance()->GetSessionContext(session_id);
  int render_view_id =
      context.guest_render_view_id == MSG_ROUTING_NONE ?
          context.render_view_id : context.guest_render_view_id;
  Send(new InputTagSpeechMsg_RecordingComplete(render_view_id,
                                               context.request_id));
  DVLOG(1) << "InputTagSpeechDispatcherHost::OnAudioEnd exit";
}

void InputTagSpeechDispatcherHost::OnRecognitionEnd(int session_id) {
  DVLOG(1) << "InputTagSpeechDispatcherHost::OnRecognitionEnd enter";
  const SpeechRecognitionSessionContext& context =
      SpeechRecognitionManager::GetInstance()->GetSessionContext(session_id);
  int render_view_id =
      context.guest_render_view_id == MSG_ROUTING_NONE ?
          context.render_view_id : context.guest_render_view_id;
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
