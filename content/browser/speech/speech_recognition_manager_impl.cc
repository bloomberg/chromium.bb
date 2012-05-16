// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/speech_recognition_manager_impl.h"

#include "base/bind.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/speech/google_one_shot_remote_engine.h"
#include "content/browser/speech/speech_recognition_engine.h"
#include "content/browser/speech/speech_recognizer_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "content/public/browser/speech_recognition_manager_delegate.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "content/public/browser/speech_recognition_session_context.h"
#include "content/public/common/speech_recognition_result.h"
#include "media/audio/audio_manager.h"

using base::Callback;
using base::Unretained;
using content::BrowserMainLoop;
using content::BrowserThread;
using content::SpeechRecognitionError;
using content::SpeechRecognitionEventListener;
using content::SpeechRecognitionManager;
using content::SpeechRecognitionResult;
using content::SpeechRecognitionSessionContext;
using content::SpeechRecognitionSessionConfig;

namespace content {
const int SpeechRecognitionManager::kSessionIDInvalid = 0;

SpeechRecognitionManager* SpeechRecognitionManager::GetInstance() {
  return speech::SpeechRecognitionManagerImpl::GetInstance();
}
}  // namespace content

namespace {
speech::SpeechRecognitionManagerImpl* g_speech_recognition_manager_impl;
}  // namespace

namespace speech {

SpeechRecognitionManagerImpl* SpeechRecognitionManagerImpl::GetInstance() {
  DCHECK(g_speech_recognition_manager_impl);
  return g_speech_recognition_manager_impl;
}

SpeechRecognitionManagerImpl::SpeechRecognitionManagerImpl()
    : session_id_capturing_audio_(kSessionIDInvalid),
      last_session_id_(kSessionIDInvalid),
      is_dispatching_event_(false),
      delegate_(content::GetContentClient()->browser()->
                    GetSpeechRecognitionManagerDelegate()) {
  DCHECK(!g_speech_recognition_manager_impl);
  g_speech_recognition_manager_impl = this;
}

SpeechRecognitionManagerImpl::~SpeechRecognitionManagerImpl() {
  DCHECK(g_speech_recognition_manager_impl);
  g_speech_recognition_manager_impl = NULL;
  // Recognition sessions will be aborted by the corresponding destructors.
  sessions_.clear();
}

int SpeechRecognitionManagerImpl::CreateSession(
    const SpeechRecognitionSessionConfig& config) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  const int session_id = GetNextSessionID();
  DCHECK(!SessionExists(session_id));
  // Set-up the new session.
  Session& session = sessions_[session_id];
  session.id = session_id;
  session.config = config;
  session.context = config.initial_context;

  std::string hardware_info;
  bool can_report_metrics = false;
  if (delegate_.get())
    delegate_->GetDiagnosticInformation(&can_report_metrics, &hardware_info);

  SpeechRecognitionEngineConfig remote_engine_config;
  remote_engine_config.language = config.language;
  remote_engine_config.grammars = config.grammars;
  remote_engine_config.audio_sample_rate =
      SpeechRecognizerImpl::kAudioSampleRate;
  remote_engine_config.audio_num_bits_per_sample =
     SpeechRecognizerImpl::kNumBitsPerAudioSample;
  remote_engine_config.filter_profanities = config.filter_profanities;
  remote_engine_config.hardware_info = hardware_info;
  remote_engine_config.origin_url = can_report_metrics ? config.origin_url : "";

  SpeechRecognitionEngine* google_remote_engine =
        new GoogleOneShotRemoteEngine(config.url_request_context_getter);
  google_remote_engine->SetConfig(remote_engine_config);

  session.recognizer = new SpeechRecognizerImpl(this,
                                                session_id,
                                                google_remote_engine);
  return session_id;
}

void SpeechRecognitionManagerImpl::StartSession(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  // If there is another active session, abort that.
  if (session_id_capturing_audio_ != kSessionIDInvalid &&
      session_id_capturing_audio_ != session_id) {
    AbortSession(session_id_capturing_audio_);
  }

  if (delegate_.get())
    delegate_->CheckRecognitionIsAllowed(
        session_id,
        base::Bind(&SpeechRecognitionManagerImpl::RecognitionAllowedCallback,
                   Unretained(this)));
}

void SpeechRecognitionManagerImpl::RecognitionAllowedCallback(int session_id,
                                                              bool is_allowed) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(SessionExists(session_id));
  if (is_allowed) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
        base::Bind(&SpeechRecognitionManagerImpl::DispatchEvent,
                   Unretained(this), session_id, EVENT_START));
  } else {
    sessions_.erase(session_id);
  }
}

void SpeechRecognitionManagerImpl::AbortSession(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&SpeechRecognitionManagerImpl::DispatchEvent, Unretained(this),
                 session_id, EVENT_ABORT));
}

void SpeechRecognitionManagerImpl::StopAudioCaptureForSession(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&SpeechRecognitionManagerImpl::DispatchEvent, Unretained(this),
                 session_id, EVENT_STOP_CAPTURE));
}

// Here begins the SpeechRecognitionEventListener interface implementation,
// which will simply relay the events to the proper listener registered for the
// particular session (most likely InputTagSpeechDispatcherHost) and to the
// catch-all listener provided by the delegate (if any).

void SpeechRecognitionManagerImpl::OnRecognitionStart(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  DCHECK_EQ(session_id_capturing_audio_, session_id);
  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnRecognitionStart(session_id);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnRecognitionStart(session_id);
}

void SpeechRecognitionManagerImpl::OnAudioStart(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  DCHECK_EQ(session_id_capturing_audio_, session_id);
  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnAudioStart(session_id);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnAudioStart(session_id);
}

void SpeechRecognitionManagerImpl::OnEnvironmentEstimationComplete(
    int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  DCHECK_EQ(session_id_capturing_audio_, session_id);
  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnEnvironmentEstimationComplete(session_id);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnEnvironmentEstimationComplete(session_id);
}

void SpeechRecognitionManagerImpl::OnSoundStart(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  DCHECK_EQ(session_id_capturing_audio_, session_id);
  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnSoundStart(session_id);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnSoundStart(session_id);
}

void SpeechRecognitionManagerImpl::OnSoundEnd(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnSoundEnd(session_id);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnSoundEnd(session_id);
}

void SpeechRecognitionManagerImpl::OnAudioEnd(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnAudioEnd(session_id);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnAudioEnd(session_id);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&SpeechRecognitionManagerImpl::DispatchEvent, Unretained(this),
                 session_id, EVENT_AUDIO_ENDED));
}

void SpeechRecognitionManagerImpl::OnRecognitionResult(
    int session_id, const content::SpeechRecognitionResult& result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnRecognitionResult(session_id, result);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnRecognitionResult(session_id, result);
}

void SpeechRecognitionManagerImpl::OnRecognitionError(
    int session_id, const content::SpeechRecognitionError& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnRecognitionError(session_id, error);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnRecognitionError(session_id, error);
}

void SpeechRecognitionManagerImpl::OnAudioLevelsChange(
    int session_id, float volume, float noise_volume) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnAudioLevelsChange(session_id, volume, noise_volume);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnAudioLevelsChange(session_id, volume, noise_volume);
}

void SpeechRecognitionManagerImpl::OnRecognitionEnd(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnRecognitionEnd(session_id);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnRecognitionEnd(session_id);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&SpeechRecognitionManagerImpl::DispatchEvent, Unretained(this),
                 session_id, EVENT_RECOGNITION_ENDED));
}

// TODO(primiano) After CL2: if we see that both InputTagDispatcherHost and
// SpeechRecognitionDispatcherHost do the same lookup operations, implement the
// lookup method directly here.
int SpeechRecognitionManagerImpl::LookupSessionByContext(
    Callback<bool(const SpeechRecognitionSessionContext&)> matcher) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  SessionsTable::const_iterator iter;
  // Note: the callback (matcher) must NEVER perform non-const calls on us.
  for(iter = sessions_.begin(); iter != sessions_.end(); ++iter) {
    const int session_id = iter->first;
    const Session& session = iter->second;
    bool matches = matcher.Run(session.context);
    if (matches)
      return session_id;
  }
  return kSessionIDInvalid;
}

SpeechRecognitionSessionContext
SpeechRecognitionManagerImpl::GetSessionContext(int session_id) const {
  return GetSession(session_id).context;
}

void SpeechRecognitionManagerImpl::AbortAllSessionsForListener(
    SpeechRecognitionEventListener* listener) {
  // This method gracefully destroys sessions for the listener. However, since
  // the listener itself is likely to be destroyed after this call, we avoid
  // dispatching further events to it, marking the |listener_is_active| flag.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  for (SessionsTable::iterator it = sessions_.begin(); it != sessions_.end();
       ++it) {
    Session& session = it->second;
    if (session.config.event_listener == listener) {
      AbortSession(session.id);
      session.listener_is_active = false;
    }
  }
}

// -----------------------  Core FSM implementation ---------------------------
void SpeechRecognitionManagerImpl::DispatchEvent(int session_id,
                                                 FSMEvent event) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  const Session& session = GetSession(session_id);
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
// All the events received by the SpeechRecognizerImpl instances (one for each
// session) are always routed to the SpeechRecognitionEventListener(s)
// regardless the choices taken in this FSM.
void SpeechRecognitionManagerImpl::ExecuteTransitionAndGetNextState(
    const Session& session, FSMState session_state, FSMEvent event) {
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
          return SessionStart(session);
        case EVENT_ABORT:
        case EVENT_RECOGNITION_ENDED:
          return SessionDelete(session);
        case EVENT_STOP_CAPTURE:
        case EVENT_AUDIO_ENDED:
          return;
      }
      break;
    case SESSION_STATE_CAPTURING_AUDIO:
      switch (event) {
        case EVENT_STOP_CAPTURE:
          return SessionStopAudioCapture(session);
        case EVENT_ABORT:
          return SessionAbort(session);
        case EVENT_START:
          return;
        case EVENT_AUDIO_ENDED:
        case EVENT_RECOGNITION_ENDED:
          return NotFeasible(session, event);
      }
      break;
    case SESSION_STATE_WAITING_FOR_RESULT:
      switch (event) {
        case EVENT_ABORT:
          return SessionAbort(session);
        case EVENT_AUDIO_ENDED:
          return ResetCapturingSessionId(session);
        case EVENT_START:
        case EVENT_STOP_CAPTURE:
          return;
        case EVENT_RECOGNITION_ENDED:
          return NotFeasible(session, event);
      }
      break;
  }
  return NotFeasible(session, event);
}

SpeechRecognitionManagerImpl::FSMState
SpeechRecognitionManagerImpl::GetSessionState(int session_id) const {
  const Session& session = GetSession(session_id);
  if (!session.recognizer.get() || !session.recognizer->IsActive())
    return SESSION_STATE_IDLE;
  if (session.recognizer->IsCapturingAudio())
    return SESSION_STATE_CAPTURING_AUDIO;
  return SESSION_STATE_WAITING_FOR_RESULT;
}

// ----------- Contract for all the FSM evolution functions below -------------
//  - Are guaranteed to be executed in the IO thread;
//  - Are guaranteed to be not reentrant (themselves and each other);

void SpeechRecognitionManagerImpl::SessionStart(const Session& session) {
  session_id_capturing_audio_ = session.id;
  session.recognizer->StartRecognition();
}

void SpeechRecognitionManagerImpl::SessionAbort(const Session& session) {
  if (session_id_capturing_audio_ == session.id)
    session_id_capturing_audio_ = kSessionIDInvalid;
  DCHECK(session.recognizer.get() && session.recognizer->IsActive());
  session.recognizer->AbortRecognition();
}

void SpeechRecognitionManagerImpl::SessionStopAudioCapture(
    const Session& session) {
  DCHECK(session.recognizer.get() && session.recognizer->IsCapturingAudio());
  session.recognizer->StopAudioCapture();
}

void SpeechRecognitionManagerImpl::ResetCapturingSessionId(
    const Session& session) {
  DCHECK_EQ(session_id_capturing_audio_, session.id);
  session_id_capturing_audio_ = kSessionIDInvalid;
}

void SpeechRecognitionManagerImpl::SessionDelete(const Session& session) {
  DCHECK(session.recognizer == NULL || !session.recognizer->IsActive());
  if (session_id_capturing_audio_ == session.id)
    session_id_capturing_audio_ = kSessionIDInvalid;
  sessions_.erase(session.id);
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

const SpeechRecognitionManagerImpl::Session&
SpeechRecognitionManagerImpl::GetSession(int session_id) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  SessionsTable::const_iterator iter = sessions_.find(session_id);
  DCHECK(iter != sessions_.end());
  return iter->second;
}

SpeechRecognitionEventListener* SpeechRecognitionManagerImpl::GetListener(
    int session_id) const {
  const Session& session = GetSession(session_id);
  return session.listener_is_active ? session.config.event_listener : NULL;
}

SpeechRecognitionEventListener*
SpeechRecognitionManagerImpl::GetDelegateListener() const {
  return delegate_.get() ? delegate_->GetEventListener() : NULL;
}

const SpeechRecognitionSessionConfig&
SpeechRecognitionManagerImpl::GetSessionConfig(int session_id) const {
  return GetSession(session_id).config;
}

bool SpeechRecognitionManagerImpl::HasAudioInputDevices() {
  return BrowserMainLoop::GetAudioManager()->HasAudioInputDevices();
}

bool SpeechRecognitionManagerImpl::IsCapturingAudio() {
  return BrowserMainLoop::GetAudioManager()->IsRecordingInProcess();
}

string16 SpeechRecognitionManagerImpl::GetAudioInputDeviceModel() {
  return BrowserMainLoop::GetAudioManager()->GetAudioInputDeviceModel();
}

void SpeechRecognitionManagerImpl::ShowAudioInputSettings() {
  // Since AudioManager::ShowAudioInputSettings can potentially launch external
  // processes, do that in the FILE thread to not block the calling threads.
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&SpeechRecognitionManagerImpl::ShowAudioInputSettings,
                   Unretained(this)));
    return;
  }

  media::AudioManager* audio_manager = BrowserMainLoop::GetAudioManager();
  DCHECK(audio_manager->CanShowAudioInputSettings());
  if (audio_manager->CanShowAudioInputSettings())
    audio_manager->ShowAudioInputSettings();
}

SpeechRecognitionManagerImpl::Session::Session()
  : id(kSessionIDInvalid),
    listener_is_active(true) {
}

SpeechRecognitionManagerImpl::Session::~Session() {
}

}  // namespace speech
