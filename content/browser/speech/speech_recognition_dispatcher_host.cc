// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/speech_recognition_dispatcher_host.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_manager.h"
#include "content/browser/speech/speech_recognition_manager_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/speech_recognition_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/speech_recognition_manager_delegate.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "content/public/browser/speech_recognition_session_context.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

SpeechRecognitionDispatcherHost::SpeechRecognitionDispatcherHost(
    int render_process_id,
    int render_frame_id,
    scoped_refptr<net::URLRequestContextGetter> context_getter,
    base::WeakPtr<IPC::Sender> sender)
    : render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      context_getter_(std::move(context_getter)),
      sender_(std::move(sender)),
      weak_factory_(this) {
  // Do not add any non-trivial initialization here, instead do it lazily when
  // required (e.g. see the method |SpeechRecognitionManager::GetInstance()|) or
  // add an Init() method.
}

// static
void SpeechRecognitionDispatcherHost::Create(
    int render_process_id,
    int render_frame_id,
    scoped_refptr<net::URLRequestContextGetter> context_getter,
    base::WeakPtr<IPC::Sender> sender,
    mojom::SpeechRecognizerRequest request) {
  mojo::MakeStrongBinding(std::make_unique<SpeechRecognitionDispatcherHost>(
                              render_process_id, render_frame_id,
                              std::move(context_getter), std::move(sender)),
                          std::move(request));
}

SpeechRecognitionDispatcherHost::~SpeechRecognitionDispatcherHost() {}

base::WeakPtr<SpeechRecognitionDispatcherHost>
SpeechRecognitionDispatcherHost::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

// -------- mojom::SpeechRecognizer interface implementation ------------------

// TODO(adithyas): Posting a task on the UI thread (and subsequently the IO
// thread) could result in a race. See https://crbug.com/836870 for details.
void SpeechRecognitionDispatcherHost::StartRequest(
    mojom::StartSpeechRecognitionRequestParamsPtr params) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Check that the origin specified by the renderer process is one
  // that it is allowed to access.
  if (!params->origin.unique() &&
      !ChildProcessSecurityPolicyImpl::GetInstance()->CanRequestURL(
          render_process_id_, params->origin.GetURL())) {
    LOG(ERROR) << "SRDH::OnStartRequest, disallowed origin: "
               << params->origin.Serialize();
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&SpeechRecognitionDispatcherHost::StartRequestOnUI,
                     AsWeakPtr(), render_process_id_, render_frame_id_,
                     std::move(params)));
}

void SpeechRecognitionDispatcherHost::AbortRequest(int32_t request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  int session_id = SpeechRecognitionManager::GetInstance()->GetSession(
      render_process_id_, render_frame_id_, request_id);

  // The renderer might provide an invalid |request_id| if the session was not
  // started as expected, e.g., due to unsatisfied security requirements.
  if (session_id != SpeechRecognitionManager::kSessionIDInvalid)
    SpeechRecognitionManager::GetInstance()->AbortSession(session_id);
}

void SpeechRecognitionDispatcherHost::AbortAllRequests() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  SpeechRecognitionManager::GetInstance()->AbortAllSessionsForRenderFrame(
      render_process_id_, render_frame_id_);
}

void SpeechRecognitionDispatcherHost::StopCaptureRequest(int32_t request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  int session_id = SpeechRecognitionManager::GetInstance()->GetSession(
      render_process_id_, render_frame_id_, request_id);

  // The renderer might provide an invalid |request_id| if the session was not
  // started as expected, e.g., due to unsatisfied security requirements.
  if (session_id != SpeechRecognitionManager::kSessionIDInvalid) {
    SpeechRecognitionManager::GetInstance()->StopAudioCaptureForSession(
        session_id);
  }
}

// -------- SpeechRecognitionEventListener interface implementation -----------

void SpeechRecognitionDispatcherHost::OnRecognitionStart(int session_id) {
  const SpeechRecognitionSessionContext& context =
      SpeechRecognitionManager::GetInstance()->GetSessionContext(session_id);
  Send(new SpeechRecognitionMsg_Started(context.render_frame_id,
                                        context.request_id));
}

void SpeechRecognitionDispatcherHost::OnAudioStart(int session_id) {
  const SpeechRecognitionSessionContext& context =
      SpeechRecognitionManager::GetInstance()->GetSessionContext(session_id);
  Send(new SpeechRecognitionMsg_AudioStarted(context.render_frame_id,
                                             context.request_id));
}

void SpeechRecognitionDispatcherHost::OnSoundStart(int session_id) {
  const SpeechRecognitionSessionContext& context =
      SpeechRecognitionManager::GetInstance()->GetSessionContext(session_id);
  Send(new SpeechRecognitionMsg_SoundStarted(context.render_frame_id,
                                             context.request_id));
}

void SpeechRecognitionDispatcherHost::OnSoundEnd(int session_id) {
  const SpeechRecognitionSessionContext& context =
      SpeechRecognitionManager::GetInstance()->GetSessionContext(session_id);
  Send(new SpeechRecognitionMsg_SoundEnded(context.render_frame_id,
                                           context.request_id));
}

void SpeechRecognitionDispatcherHost::OnAudioEnd(int session_id) {
  const SpeechRecognitionSessionContext& context =
      SpeechRecognitionManager::GetInstance()->GetSessionContext(session_id);
  Send(new SpeechRecognitionMsg_AudioEnded(context.render_frame_id,
                                           context.request_id));
}

void SpeechRecognitionDispatcherHost::OnRecognitionEnd(int session_id) {
  const SpeechRecognitionSessionContext& context =
      SpeechRecognitionManager::GetInstance()->GetSessionContext(session_id);
  Send(new SpeechRecognitionMsg_Ended(context.render_frame_id,
                                      context.request_id));
}

void SpeechRecognitionDispatcherHost::OnRecognitionResults(
    int session_id,
    const SpeechRecognitionResults& results) {
  const SpeechRecognitionSessionContext& context =
      SpeechRecognitionManager::GetInstance()->GetSessionContext(session_id);
  Send(new SpeechRecognitionMsg_ResultRetrieved(context.render_frame_id,
                                                context.request_id, results));
}

void SpeechRecognitionDispatcherHost::OnRecognitionError(
    int session_id,
    const SpeechRecognitionError& error) {
  const SpeechRecognitionSessionContext& context =
      SpeechRecognitionManager::GetInstance()->GetSessionContext(session_id);
  Send(new SpeechRecognitionMsg_ErrorOccurred(context.render_frame_id,
                                              context.request_id, error));
}

// The events below are currently not used by speech JS APIs implementation.
void SpeechRecognitionDispatcherHost::OnAudioLevelsChange(int session_id,
                                                          float volume,
                                                          float noise_volume) {}

void SpeechRecognitionDispatcherHost::OnEnvironmentEstimationComplete(
    int session_id) {}

// static
void SpeechRecognitionDispatcherHost::StartRequestOnUI(
    base::WeakPtr<SpeechRecognitionDispatcherHost>
        speech_recognition_dispatcher_host,
    int render_process_id,
    int render_frame_id,
    mojom::StartSpeechRecognitionRequestParamsPtr params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  int embedder_render_process_id = 0;
  int embedder_render_frame_id = MSG_ROUTING_NONE;

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(WebContentsImpl::FromRenderFrameHostID(
          render_process_id, render_frame_id));
  if (!web_contents) {
    // The render frame id is renderer-provided. If it's invalid, don't crash.
    DLOG(ERROR) << "SRDH::OnStartRequest, invalid frame";
    return;
  }

  // If the speech API request was from an inner WebContents or a guest, save
  // the context of the outer WebContents or the embedder since we will use it
  // to decide permission.
  WebContents* outer_web_contents = web_contents->GetOuterWebContents();
  if (outer_web_contents) {
    RenderFrameHost* embedder_frame = nullptr;

    FrameTreeNode* embedder_frame_node = web_contents->GetMainFrame()
                                             ->frame_tree_node()
                                             ->render_manager()
                                             ->GetOuterDelegateNode();
    if (embedder_frame_node) {
      embedder_frame = embedder_frame_node->current_frame_host();
    } else {
      // The outer web contents is embedded using the browser plugin. Fall back
      // to a simple lookup of the main frame. TODO(avi): When the browser
      // plugin is retired, remove this code.
      embedder_frame = outer_web_contents->GetMainFrame();
    }

    embedder_render_process_id = embedder_frame->GetProcess()->GetID();
    DCHECK_NE(embedder_render_process_id, 0);
    embedder_render_frame_id = embedder_frame->GetRoutingID();
    DCHECK_NE(embedder_render_frame_id, MSG_ROUTING_NONE);
  }

  bool filter_profanities =
      SpeechRecognitionManagerImpl::GetInstance() &&
      SpeechRecognitionManagerImpl::GetInstance()->delegate() &&
      SpeechRecognitionManagerImpl::GetInstance()
          ->delegate()
          ->FilterProfanities(embedder_render_process_id);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&SpeechRecognitionDispatcherHost::StartSessionOnIO,
                     speech_recognition_dispatcher_host, std::move(params),
                     embedder_render_process_id, embedder_render_frame_id,
                     filter_profanities));
}

void SpeechRecognitionDispatcherHost::StartSessionOnIO(
    mojom::StartSpeechRecognitionRequestParamsPtr params,
    int embedder_render_process_id,
    int embedder_render_frame_id,
    bool filter_profanities) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  SpeechRecognitionSessionContext context;
  context.security_origin = params->origin;
  context.render_process_id = render_process_id_;
  context.render_frame_id = render_frame_id_;
  context.embedder_render_process_id = embedder_render_process_id;
  context.embedder_render_frame_id = embedder_render_frame_id;
  context.request_id = params->request_id;

  SpeechRecognitionSessionConfig config;
  config.language = params->language;
  config.grammars = params->grammars;
  config.max_hypotheses = params->max_hypotheses;
  config.origin = params->origin;
  config.initial_context = context;
  config.url_request_context_getter = context_getter_.get();
  config.filter_profanities = filter_profanities;
  config.continuous = params->continuous;
  config.interim_results = params->interim_results;
  config.event_listener = AsWeakPtr();

  int session_id =
      SpeechRecognitionManager::GetInstance()->CreateSession(config);
  DCHECK_NE(session_id, SpeechRecognitionManager::kSessionIDInvalid);
  SpeechRecognitionManager::GetInstance()->StartSession(session_id);
}

void SpeechRecognitionDispatcherHost::Send(IPC::Message* message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // The weak ptr must only be accessed on the UI thread.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(base::IgnoreResult(&IPC::Sender::Send), sender_, message));
}

}  // namespace content
