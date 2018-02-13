// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/speech_recognition_manager_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/browser/speech/speech_recognition_engine.h"
#include "content/browser/speech/speech_recognizer_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "content/public/browser/speech_recognition_manager_delegate.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "content/public/browser/speech_recognition_session_context.h"
#include "content/public/common/speech_recognition_error.h"
#include "content/public/common/speech_recognition_result.h"
#include "media/audio/audio_device_description.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(OS_ANDROID)
#include "content/browser/speech/speech_recognizer_impl_android.h"
#endif

using base::Callback;

namespace content {

SpeechRecognitionManager* SpeechRecognitionManager::manager_for_tests_;

namespace {

SpeechRecognitionManagerImpl* g_speech_recognition_manager_impl;

}  // namespace

SpeechRecognitionManager* SpeechRecognitionManager::GetInstance() {
  if (manager_for_tests_)
    return manager_for_tests_;
  return SpeechRecognitionManagerImpl::GetInstance();
}

void SpeechRecognitionManager::SetManagerForTesting(
    SpeechRecognitionManager* manager) {
  manager_for_tests_ = manager;
}

SpeechRecognitionManagerImpl* SpeechRecognitionManagerImpl::GetInstance() {
  return g_speech_recognition_manager_impl;
}

SpeechRecognitionManagerImpl::SpeechRecognitionManagerImpl(
    media::AudioSystem* audio_system,
    media::AudioManager* audio_manager,
    MediaStreamManager* media_stream_manager)
    : audio_system_(audio_system),
      audio_manager_(audio_manager),
      media_stream_manager_(media_stream_manager),
      primary_session_id_(kSessionIDInvalid),
      last_session_id_(kSessionIDInvalid),
      is_dispatching_event_(false),
      delegate_(GetContentClient()
                    ->browser()
                    ->CreateSpeechRecognitionManagerDelegate()),
      weak_factory_(this) {
  DCHECK(!g_speech_recognition_manager_impl);
  g_speech_recognition_manager_impl = this;
}

SpeechRecognitionManagerImpl::~SpeechRecognitionManagerImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(g_speech_recognition_manager_impl);

  g_speech_recognition_manager_impl = nullptr;
}

int SpeechRecognitionManagerImpl::CreateSession(
    const SpeechRecognitionSessionConfig& config) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const int session_id = GetNextSessionID();
  DCHECK(!SessionExists(session_id));

  // Set-up the new session.
  auto session = std::make_unique<Session>();
  session->id = session_id;
  session->config = config;
  session->context = config.initial_context;

#if !defined(OS_ANDROID)
  // A SpeechRecognitionEngine (and corresponding Config) is required only
  // when using SpeechRecognizerImpl, which performs the audio capture and
  // endpointing in the browser. This is not the case of Android where, not
  // only the speech recognition, but also the audio capture and endpointing
  // activities performed outside of the browser (delegated via JNI to the
  // Android API implementation).

  SpeechRecognitionEngine::Config remote_engine_config;
  remote_engine_config.language = config.language;
  remote_engine_config.grammars = config.grammars;
  remote_engine_config.audio_sample_rate =
      SpeechRecognizerImpl::kAudioSampleRate;
  remote_engine_config.audio_num_bits_per_sample =
      SpeechRecognizerImpl::kNumBitsPerAudioSample;
  remote_engine_config.filter_profanities = config.filter_profanities;
  remote_engine_config.continuous = config.continuous;
  remote_engine_config.interim_results = config.interim_results;
  remote_engine_config.max_hypotheses = config.max_hypotheses;
  remote_engine_config.origin_url = config.origin_url;
  remote_engine_config.auth_token = config.auth_token;
  remote_engine_config.auth_scope = config.auth_scope;
  remote_engine_config.preamble = config.preamble;

  SpeechRecognitionEngine* google_remote_engine =
      new SpeechRecognitionEngine(config.url_request_context_getter.get());
  google_remote_engine->SetConfig(remote_engine_config);

  session->recognizer = new SpeechRecognizerImpl(
      this, audio_system_, audio_manager_, session_id, config.continuous,
      config.interim_results, google_remote_engine);
#else
  session->recognizer = new SpeechRecognizerImplAndroid(this, session_id);
#endif

  sessions_[session_id] = std::move(session);

  return session_id;
}

void SpeechRecognitionManagerImpl::StartSession(int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!SessionExists(session_id))
    return;

  // If there is another active session, abort that.
  if (primary_session_id_ != kSessionIDInvalid &&
      primary_session_id_ != session_id) {
    AbortSession(primary_session_id_);
  }

  primary_session_id_ = session_id;

  if (delegate_) {
    delegate_->CheckRecognitionIsAllowed(
        session_id,
        base::BindOnce(
            &SpeechRecognitionManagerImpl::RecognitionAllowedCallback,
            weak_factory_.GetWeakPtr(), session_id));
  }
}

void SpeechRecognitionManagerImpl::RecognitionAllowedCallback(int session_id,
                                                              bool ask_user,
                                                              bool is_allowed) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!SessionExists(session_id))
    return;

  auto iter = sessions_.find(session_id);
  DCHECK(iter != sessions_.end());
  Session* session = iter->second.get();

  if (session->abort_requested)
    return;

  if (ask_user) {
    SpeechRecognitionSessionContext& context = session->context;
    context.label = media_stream_manager_->MakeMediaAccessRequest(
        context.render_process_id, context.render_frame_id, context.request_id,
        StreamControls(true, false),
        url::Origin::Create(GURL(context.context_name)),
        base::BindOnce(
            &SpeechRecognitionManagerImpl::MediaRequestPermissionCallback,
            weak_factory_.GetWeakPtr(), session_id));
    return;
  }

  if (is_allowed) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&SpeechRecognitionManagerImpl::DispatchEvent,
                       weak_factory_.GetWeakPtr(), session_id, EVENT_START));
  } else {
    OnRecognitionError(session_id, SpeechRecognitionError(
        SPEECH_RECOGNITION_ERROR_NOT_ALLOWED));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&SpeechRecognitionManagerImpl::DispatchEvent,
                       weak_factory_.GetWeakPtr(), session_id, EVENT_ABORT));
  }
}

void SpeechRecognitionManagerImpl::MediaRequestPermissionCallback(
    int session_id,
    const MediaStreamDevices& devices,
    std::unique_ptr<MediaStreamUIProxy> stream_ui) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto iter = sessions_.find(session_id);
  if (iter == sessions_.end())
    return;

  bool is_allowed = !devices.empty();
  if (is_allowed) {
    // Copy the approved devices array to the context for UI indication.
    iter->second->context.devices = devices;

    // Save the UI object.
    iter->second->ui = std::move(stream_ui);
  }

  // Clear the label to indicate the request has been done.
  iter->second->context.label.clear();

  // Notify the recognition about the request result.
  RecognitionAllowedCallback(iter->first, false, is_allowed);
}

void SpeechRecognitionManagerImpl::AbortSession(int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!SessionExists(session_id))
    return;

  auto iter = sessions_.find(session_id);
  iter->second->ui.reset();

  if (iter->second->abort_requested)
    return;

  iter->second->abort_requested = true;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&SpeechRecognitionManagerImpl::DispatchEvent,
                     weak_factory_.GetWeakPtr(), session_id, EVENT_ABORT));
}

void SpeechRecognitionManagerImpl::StopAudioCaptureForSession(int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!SessionExists(session_id))
    return;

  auto iter = sessions_.find(session_id);
  iter->second->ui.reset();

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&SpeechRecognitionManagerImpl::DispatchEvent,
                                weak_factory_.GetWeakPtr(), session_id,
                                EVENT_STOP_CAPTURE));
}

// Here begins the SpeechRecognitionEventListener interface implementation,
// which will simply relay the events to the proper listener registered for the
// particular session and to the catch-all listener provided by the delegate
// (if any).

void SpeechRecognitionManagerImpl::OnRecognitionStart(int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!SessionExists(session_id))
    return;

  auto iter = sessions_.find(session_id);
  if (iter->second->ui) {
    // Notify the UI that the devices are being used.
    iter->second->ui->OnStarted(base::OnceClosure(),
                                MediaStreamUIProxy::WindowIdCallback());
  }

  DCHECK_EQ(primary_session_id_, session_id);
  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnRecognitionStart(session_id);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnRecognitionStart(session_id);
}

void SpeechRecognitionManagerImpl::OnAudioStart(int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!SessionExists(session_id))
    return;

  DCHECK_EQ(primary_session_id_, session_id);
  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnAudioStart(session_id);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnAudioStart(session_id);
}

void SpeechRecognitionManagerImpl::OnEnvironmentEstimationComplete(
    int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!SessionExists(session_id))
    return;

  DCHECK_EQ(primary_session_id_, session_id);
  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnEnvironmentEstimationComplete(session_id);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnEnvironmentEstimationComplete(session_id);
}

void SpeechRecognitionManagerImpl::OnSoundStart(int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!SessionExists(session_id))
    return;

  DCHECK_EQ(primary_session_id_, session_id);
  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnSoundStart(session_id);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnSoundStart(session_id);
}

void SpeechRecognitionManagerImpl::OnSoundEnd(int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!SessionExists(session_id))
    return;

  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnSoundEnd(session_id);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnSoundEnd(session_id);
}

void SpeechRecognitionManagerImpl::OnAudioEnd(int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!SessionExists(session_id))
    return;

  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnAudioEnd(session_id);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnAudioEnd(session_id);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&SpeechRecognitionManagerImpl::DispatchEvent,
                                weak_factory_.GetWeakPtr(), session_id,
                                EVENT_AUDIO_ENDED));
}

void SpeechRecognitionManagerImpl::OnRecognitionResults(
    int session_id, const SpeechRecognitionResults& results) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!SessionExists(session_id))
    return;

  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnRecognitionResults(session_id, results);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnRecognitionResults(session_id, results);
}

void SpeechRecognitionManagerImpl::OnRecognitionError(
    int session_id, const SpeechRecognitionError& error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!SessionExists(session_id))
    return;

  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnRecognitionError(session_id, error);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnRecognitionError(session_id, error);
}

void SpeechRecognitionManagerImpl::OnAudioLevelsChange(
    int session_id, float volume, float noise_volume) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!SessionExists(session_id))
    return;

  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnAudioLevelsChange(session_id, volume, noise_volume);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnAudioLevelsChange(session_id, volume, noise_volume);
}

void SpeechRecognitionManagerImpl::OnRecognitionEnd(int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!SessionExists(session_id))
    return;

  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnRecognitionEnd(session_id);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnRecognitionEnd(session_id);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&SpeechRecognitionManagerImpl::DispatchEvent,
                                weak_factory_.GetWeakPtr(), session_id,
                                EVENT_RECOGNITION_ENDED));
}

int SpeechRecognitionManagerImpl::GetSession(
    int render_process_id, int render_view_id, int request_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto iter = std::find_if(
      sessions_.begin(), sessions_.end(),
      [render_process_id, render_view_id, request_id](
          const std::pair<int, std::unique_ptr<Session>>& session_pair) {
        const SpeechRecognitionSessionContext& context =
            session_pair.second->context;
        return context.render_process_id == render_process_id &&
               context.render_view_id == render_view_id &&
               context.request_id == request_id;
      });
  if (iter == sessions_.end())
    return kSessionIDInvalid;

  return iter->first;
}

SpeechRecognitionSessionContext
SpeechRecognitionManagerImpl::GetSessionContext(int session_id) const {
  return GetSession(session_id)->context;
}

void SpeechRecognitionManagerImpl::AbortAllSessionsForRenderProcess(
    int render_process_id) {
  // This method gracefully destroys sessions for the listener. However, since
  // the listener itself is likely to be destroyed after this call, we avoid
  // dispatching further events to it, marking the |listener_is_active| flag.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (const auto& session_pair : sessions_) {
    Session* session = session_pair.second.get();
    if (session->context.render_process_id == render_process_id) {
      AbortSession(session->id);
      session->listener_is_active = false;
    }
  }
}

void SpeechRecognitionManagerImpl::AbortAllSessionsForRenderView(
    int render_process_id,
    int render_view_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (const auto& session_pair : sessions_) {
    Session* session = session_pair.second.get();
    if (session->context.render_process_id == render_process_id &&
        session->context.render_view_id == render_view_id) {
      AbortSession(session->id);
    }
  }
}

// -----------------------  Core FSM implementation ---------------------------
void SpeechRecognitionManagerImpl::DispatchEvent(int session_id,
                                                 FSMEvent event) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // There are some corner cases in which the session might be deleted (due to
  // an EndRecognition event) between a request (e.g. Abort) and its dispatch.
  if (!SessionExists(session_id))
    return;

  Session* session = GetSession(session_id);
  FSMState session_state = GetSessionState(session_id);
  DCHECK_LE(session_state, SESSION_STATE_MAX_VALUE);
  DCHECK_LE(event, EVENT_MAX_VALUE);

  // Event dispatching must be sequential, otherwise it will break all the rules
  // and the assumptions of the finite state automata model.
  DCHECK(!is_dispatching_event_);
  is_dispatching_event_ = true;
  ExecuteTransitionAndGetNextState(session, session_state, event);
  is_dispatching_event_ = false;
}

// This FSM handles the evolution of each session, from the viewpoint of the
// interaction with the user (that may be either the browser end-user which
// interacts with UI bubbles, or JS developer intracting with JS methods).
// All the events received by the SpeechRecognizer instances (one for each
// session) are always routed to the SpeechRecognitionEventListener(s)
// regardless the choices taken in this FSM.
void SpeechRecognitionManagerImpl::ExecuteTransitionAndGetNextState(
    Session* session, FSMState session_state, FSMEvent event) {
  // Note: since we're not tracking the state of the recognizer object, rather
  // we're directly retrieving it (through GetSessionState), we see its events
  // (that are AUDIO_ENDED and RECOGNITION_ENDED) after its state evolution
  // (e.g., when we receive the AUDIO_ENDED event, the recognizer has just
  // completed the transition from CAPTURING_AUDIO to WAITING_FOR_RESULT, thus
  // we perceive the AUDIO_ENDED event in WAITING_FOR_RESULT).
  // This makes the code below a bit tricky but avoids a lot of code for
  // tracking and reconstructing asynchronously the state of the recognizer.
  switch (session_state) {
    case SESSION_STATE_IDLE:
      switch (event) {
        case EVENT_START:
          return SessionStart(*session);
        case EVENT_ABORT:
          return SessionAbort(*session);
        case EVENT_RECOGNITION_ENDED:
          return SessionDelete(session);
        case EVENT_STOP_CAPTURE:
          return SessionStopAudioCapture(*session);
        case EVENT_AUDIO_ENDED:
          return;
      }
      break;
    case SESSION_STATE_CAPTURING_AUDIO:
      switch (event) {
        case EVENT_STOP_CAPTURE:
          return SessionStopAudioCapture(*session);
        case EVENT_ABORT:
          return SessionAbort(*session);
        case EVENT_START:
          return;
        case EVENT_AUDIO_ENDED:
        case EVENT_RECOGNITION_ENDED:
          return NotFeasible(*session, event);
      }
      break;
    case SESSION_STATE_WAITING_FOR_RESULT:
      switch (event) {
        case EVENT_ABORT:
          return SessionAbort(*session);
        case EVENT_AUDIO_ENDED:
          return ResetCapturingSessionId(*session);
        case EVENT_START:
        case EVENT_STOP_CAPTURE:
          return;
        case EVENT_RECOGNITION_ENDED:
          return NotFeasible(*session, event);
      }
      break;
  }
  return NotFeasible(*session, event);
}

SpeechRecognitionManagerImpl::FSMState
SpeechRecognitionManagerImpl::GetSessionState(int session_id) const {
  Session* session = GetSession(session_id);
  if (!session->recognizer.get() || !session->recognizer->IsActive())
    return SESSION_STATE_IDLE;
  if (session->recognizer->IsCapturingAudio())
    return SESSION_STATE_CAPTURING_AUDIO;
  return SESSION_STATE_WAITING_FOR_RESULT;
}

// ----------- Contract for all the FSM evolution functions below -------------
//  - Are guaranteed to be executed in the IO thread;
//  - Are guaranteed to be not reentrant (themselves and each other);

void SpeechRecognitionManagerImpl::SessionStart(const Session& session) {
  DCHECK_EQ(primary_session_id_, session.id);
  const MediaStreamDevices& devices = session.context.devices;
  std::string device_id;
  if (devices.empty()) {
    // From the ask_user=false path, use the default device.
    // TODO(xians): Abort the session after we do not need to support this path
    // anymore.
    device_id = media::AudioDeviceDescription::kDefaultDeviceId;
  } else {
    // From the ask_user=true path, use the selected device.
    DCHECK_EQ(1u, devices.size());
    DCHECK_EQ(MEDIA_DEVICE_AUDIO_CAPTURE, devices.front().type);
    device_id = devices.front().id;
  }

  session.recognizer->StartRecognition(device_id);
}

void SpeechRecognitionManagerImpl::SessionAbort(const Session& session) {
  if (primary_session_id_ == session.id)
    primary_session_id_ = kSessionIDInvalid;
  DCHECK(session.recognizer.get());
  session.recognizer->AbortRecognition();
}

void SpeechRecognitionManagerImpl::SessionStopAudioCapture(
    const Session& session) {
  DCHECK(session.recognizer.get());
  session.recognizer->StopAudioCapture();
}

void SpeechRecognitionManagerImpl::ResetCapturingSessionId(
    const Session& session) {
  DCHECK_EQ(primary_session_id_, session.id);
  primary_session_id_ = kSessionIDInvalid;
}

void SpeechRecognitionManagerImpl::SessionDelete(Session* session) {
  DCHECK(session->recognizer.get() == nullptr ||
         !session->recognizer->IsActive());
  if (primary_session_id_ == session->id)
    primary_session_id_ = kSessionIDInvalid;
  if (!session->context.label.empty())
    media_stream_manager_->CancelRequest(session->context.label);
  sessions_.erase(session->id);
}

void SpeechRecognitionManagerImpl::NotFeasible(const Session& session,
                                               FSMEvent event) {
  NOTREACHED() << "Unfeasible event " << event
               << " in state " << GetSessionState(session.id)
               << " for session " << session.id;
}

int SpeechRecognitionManagerImpl::GetNextSessionID() {
  ++last_session_id_;
  // Deal with wrapping of last_session_id_. (How civilized).
  if (last_session_id_ <= 0)
    last_session_id_ = 1;
  return last_session_id_;
}

bool SpeechRecognitionManagerImpl::SessionExists(int session_id) const {
  return sessions_.find(session_id) != sessions_.end();
}

SpeechRecognitionManagerImpl::Session*
SpeechRecognitionManagerImpl::GetSession(int session_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto iter = sessions_.find(session_id);
  DCHECK(iter != sessions_.end());
  return iter->second.get();
}

SpeechRecognitionEventListener* SpeechRecognitionManagerImpl::GetListener(
    int session_id) const {
  Session* session = GetSession(session_id);
  if (session->listener_is_active && session->config.event_listener)
    return session->config.event_listener.get();
  return nullptr;
}

SpeechRecognitionEventListener*
SpeechRecognitionManagerImpl::GetDelegateListener() const {
  return delegate_.get() ? delegate_->GetEventListener() : nullptr;
}

const SpeechRecognitionSessionConfig&
SpeechRecognitionManagerImpl::GetSessionConfig(int session_id) const {
  return GetSession(session_id)->config;
}

SpeechRecognitionManagerImpl::Session::Session()
  : id(kSessionIDInvalid),
    abort_requested(false),
    listener_is_active(true) {
}

SpeechRecognitionManagerImpl::Session::~Session() {
}

}  // namespace content
