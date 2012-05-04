// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/speech_recognition_manager_impl.h"

#include "base/bind.h"
#include "base/memory/singleton.h"
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

namespace speech {

SpeechRecognitionManagerImpl* SpeechRecognitionManagerImpl::GetInstance() {
  return Singleton<SpeechRecognitionManagerImpl,
                   LeakySingletonTraits<SpeechRecognitionManagerImpl> >::get();
}

SpeechRecognitionManagerImpl::SpeechRecognitionManagerImpl()
    : interactive_session_id_(kSessionIDInvalid),
      last_session_id_(kSessionIDInvalid),
      is_dispatching_event_(false) {
  delegate_ = content::GetContentClient()->browser()->
      GetSpeechRecognitionManagerDelegate();
}

SpeechRecognitionManagerImpl::~SpeechRecognitionManagerImpl() {
  // Recognition sessions will be aborted by the corresponding destructors.
  sessions_.clear();
}

int SpeechRecognitionManagerImpl::CreateSession(
    const SpeechRecognitionSessionConfig& config,
    SpeechRecognitionEventListener* event_listener) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  const int session_id = GetNextSessionID();
  DCHECK(!SessionExists(session_id));
  // Set-up the new session.
  Session& session = sessions_[session_id];
  session.id = session_id;
  session.event_listener = event_listener;
  session.context = config.initial_context;

  std::string hardware_info;
  bool can_report_metrics = false;
  if (delegate_)
    delegate_->GetDiagnosticInformation(&can_report_metrics, &hardware_info);

  GoogleOneShotRemoteEngineConfig remote_engine_config;
  remote_engine_config.language = config.language;
  remote_engine_config.grammar = config.grammar;
  remote_engine_config.audio_sample_rate =
      SpeechRecognizerImpl::kAudioSampleRate;
  remote_engine_config.audio_num_bits_per_sample =
     SpeechRecognizerImpl::kNumBitsPerAudioSample;
  remote_engine_config.filter_profanities = config.filter_profanities;
  remote_engine_config.hardware_info = hardware_info;
  remote_engine_config.origin_url = can_report_metrics ? config.origin_url : "";

  GoogleOneShotRemoteEngine* google_remote_engine =
      new GoogleOneShotRemoteEngine(config.url_request_context_getter);
  google_remote_engine->SetConfig(remote_engine_config);

  session.recognizer = new SpeechRecognizerImpl(this,
                                                session_id,
                                                google_remote_engine);
  return session_id;
}

void SpeechRecognitionManagerImpl::StartSession(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(SessionExists(session_id));

  // If there is another interactive session, send it to background.
  if (interactive_session_id_ != kSessionIDInvalid &&
      interactive_session_id_ != session_id) {
     SendSessionToBackground(interactive_session_id_);
  }

  if (delegate_)
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
                   Unretained(this), session_id, FSMEventArgs(EVENT_START)));
  } else {
    sessions_.erase(session_id);
  }
}

void SpeechRecognitionManagerImpl::AbortSession(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(SessionExists(session_id));

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&SpeechRecognitionManagerImpl::DispatchEvent, Unretained(this),
                 session_id, FSMEventArgs(EVENT_ABORT)));
}

void SpeechRecognitionManagerImpl::StopAudioCaptureForSession(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(SessionExists(session_id));

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&SpeechRecognitionManagerImpl::DispatchEvent, Unretained(this),
                 session_id, FSMEventArgs(EVENT_STOP_CAPTURE)));
}

void SpeechRecognitionManagerImpl::SendSessionToBackground(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(SessionExists(session_id));

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&SpeechRecognitionManagerImpl::DispatchEvent, Unretained(this),
                 session_id, FSMEventArgs(EVENT_SET_BACKGROUND)));
}

// Here begins the SpeechRecognitionEventListener interface implementation,
// which will simply relay the events to the proper listener registered for the
// particular session (most likely InputTagSpeechDispatcherHost) and intercept
// some of them to provide UI notifications.

void SpeechRecognitionManagerImpl::OnRecognitionStart(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  DCHECK_EQ(interactive_session_id_, session_id);
  if (delegate_)
    delegate_->ShowWarmUp(session_id);
  GetListener(session_id)->OnRecognitionStart(session_id);
}

void SpeechRecognitionManagerImpl::OnAudioStart(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  DCHECK_EQ(interactive_session_id_, session_id);
  if (delegate_)
    delegate_->ShowRecording(session_id);
  GetListener(session_id)->OnAudioStart(session_id);
}

void SpeechRecognitionManagerImpl::OnEnvironmentEstimationComplete(
    int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  DCHECK_EQ(interactive_session_id_, session_id);
  GetListener(session_id)->OnEnvironmentEstimationComplete(session_id);
}

void SpeechRecognitionManagerImpl::OnSoundStart(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  DCHECK_EQ(interactive_session_id_, session_id);
  GetListener(session_id)->OnSoundStart(session_id);
}

void SpeechRecognitionManagerImpl::OnSoundEnd(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  GetListener(session_id)->OnSoundEnd(session_id);
}

void SpeechRecognitionManagerImpl::OnAudioEnd(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  // OnAudioEnd can also be raised after an abort request, when the session is
  // not interactive anymore.
  if (interactive_session_id_ == session_id && delegate_)
    delegate_->ShowRecognizing(session_id);

  GetListener(session_id)->OnAudioEnd(session_id);
}

void SpeechRecognitionManagerImpl::OnRecognitionResult(
    int session_id, const content::SpeechRecognitionResult& result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  GetListener(session_id)->OnRecognitionResult(session_id, result);
  FSMEventArgs event_args(EVENT_RECOGNITION_RESULT);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
    base::Bind(&SpeechRecognitionManagerImpl::DispatchEvent, Unretained(this),
               session_id, event_args));
}

void SpeechRecognitionManagerImpl::OnRecognitionError(
    int session_id, const content::SpeechRecognitionError& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  GetListener(session_id)->OnRecognitionError(session_id, error);
  FSMEventArgs event_args(EVENT_RECOGNITION_ERROR);
  event_args.speech_error = error;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
    base::Bind(&SpeechRecognitionManagerImpl::DispatchEvent, Unretained(this),
               session_id, event_args));
}

void SpeechRecognitionManagerImpl::OnAudioLevelsChange(
    int session_id, float volume, float noise_volume) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  if (delegate_)
    delegate_->ShowInputVolume(session_id, volume, noise_volume);

  GetListener(session_id)->OnAudioLevelsChange(session_id, volume,
                                               noise_volume);
}

void SpeechRecognitionManagerImpl::OnRecognitionEnd(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  GetListener(session_id)->OnRecognitionEnd(session_id);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
    base::Bind(&SpeechRecognitionManagerImpl::DispatchEvent, Unretained(this),
               session_id, FSMEventArgs(EVENT_RECOGNITION_ENDED)));
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  SessionsTable::const_iterator iter = sessions_.find(session_id);
  DCHECK(iter != sessions_.end());
  return iter->second.context;
}

void SpeechRecognitionManagerImpl::AbortAllSessionsForListener(
    SpeechRecognitionEventListener* listener) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // This method ungracefully destroys sessions (and the underlying recognizer)
  // for the listener. There is no time to call Abort (that is asynchronous) on
  // them since the listener itself is likely to be destroyed after the call,
  // and in that case we won't deliver events to a freed listener.
  // Thus we assume that the dtors of Sessions (and in turn dtors of all
  // contained objects) are designed to dispose resources cleanly.
  std::vector<int> sessions_for_listener;

  // Copy coressponding session ids.
  for (SessionsTable::iterator it = sessions_.begin(); it != sessions_.end();
       ++it) {
    if (it->second.event_listener == listener)
      sessions_for_listener.push_back(it->first);
  }
  // Remove them.
  for (size_t i = 0; i < sessions_for_listener.size(); ++i) {
    const int session_id = sessions_for_listener[i];
    if (interactive_session_id_ == session_id)
      interactive_session_id_ = kSessionIDInvalid;
    sessions_.erase(session_id);
  }
}

// -----------------------  Core FSM implementation ---------------------------
void SpeechRecognitionManagerImpl::DispatchEvent(int session_id,
                                                 FSMEventArgs event_args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  Session& session = sessions_[session_id];
  DCHECK_LE(session.state, STATE_MAX_VALUE);
  DCHECK_LE(event_args.event, EVENT_MAX_VALUE);

  // Event dispatching must be sequential, otherwise it will break all the rules
  // and the assumptions of the finite state automata model.
  DCHECK(!is_dispatching_event_);
  is_dispatching_event_ = true;

  // Pedantic preconditions consistency checks.
  if (session.state == STATE_INTERACTIVE)
    DCHECK_EQ(interactive_session_id_, session_id);

  if (session.state == STATE_BACKGROUND ||
      session.state == STATE_WAITING_FOR_DELETION) {
    DCHECK_NE(interactive_session_id_, session_id);
  }

  FSMState next_state = ExecuteTransitionAndGetNextState(session, event_args);
  if (SessionExists(session_id))  // Session might be deleted.
    session.state = next_state;

  is_dispatching_event_ = false;
}

// This FSM handles the evolution of each session, from the viewpoint of the
// interaction with the user (that may be either the browser end-user which
// interacts with UI bubbles, or JS developer intracting with JS methods).
// All the events received by the SpeechRecognizerImpl instances (one for each
// session) are always routed to the SpeechRecognitionEventListener(s)
// regardless the choices taken in this FSM.
SpeechRecognitionManagerImpl::FSMState
SpeechRecognitionManagerImpl::ExecuteTransitionAndGetNextState(
    Session& session, const FSMEventArgs& event_args) {
  // Some notes for the code below:
  // - A session can be deleted only if it is not active, thus only if it ended
  //   spontaneously or we issued a prior SessionAbort. In these cases, we must
  //   wait for a RECOGNITION_ENDED event (which is guaranteed to come always at
  //   last by the SpeechRecognizer) in order to free resources gracefully.
  // - Use SessionDelete only when absolutely sure that the recognizer is not
  //   active. Prefer SessionAbort, which will do it gracefully, otherwise.
  // - Since this class methods are publicly exported,  START, ABORT,
  //   STOP_CAPTURE and SET_BACKGROUND events can arrive in every moment from
  //   the outside wild wolrd, even if they make no sense.
  const FSMEvent event = event_args.event;
  switch (session.state) {
    case STATE_IDLE:
      // Session has just been created or had an error while interactive.
      switch (event) {
        case EVENT_START:
          return SessionStart(session, event_args);
        case EVENT_ABORT:
        case EVENT_SET_BACKGROUND:
          return SessionAbort(session, event_args);
        case EVENT_STOP_CAPTURE:
        case EVENT_RECOGNITION_ENDED:
          // In case of error, we come back in this state before receiving the
          // OnRecognitionEnd event, thus EVENT_RECOGNITION_ENDED is feasible.
          return DoNothing(session, event_args);
        case EVENT_RECOGNITION_RESULT:
        case EVENT_RECOGNITION_ERROR:
          return NotFeasible(session, event_args);
      }
      break;
    case STATE_INTERACTIVE:
      // The recognizer can be either capturing audio or waiting for a result.
      switch (event) {
        case EVENT_RECOGNITION_RESULT:
          // TODO(primiano) Valid only in single shot mode. Review in next CLs.
          return SessionSetBackground(session, event_args);
        case EVENT_SET_BACKGROUND:
          return SessionAbortIfCapturingAudioOrBackground(session, event_args);
        case EVENT_STOP_CAPTURE:
          return SessionStopAudioCapture(session, event_args);
        case EVENT_ABORT:
          return SessionAbort(session, event_args);
        case EVENT_RECOGNITION_ERROR:
          return SessionReportError(session, event_args);
        case EVENT_RECOGNITION_ENDED:
          // If we're still interactive it means that no result was received
          // in the meanwhile (otherwise we'd have been sent to background).
          return SessionReportNoMatch(session, event_args);
        case EVENT_START:
          return DoNothing(session, event_args);
      }
      break;
    case STATE_BACKGROUND:
      switch (event) {
        case EVENT_ABORT:
          return SessionAbort(session, event_args);
        case EVENT_RECOGNITION_ENDED:
          return SessionDelete(session, event_args);
        case EVENT_START:
        case EVENT_STOP_CAPTURE:
        case EVENT_RECOGNITION_RESULT:
        case EVENT_RECOGNITION_ERROR:
          return DoNothing(session, event_args);
        case EVENT_SET_BACKGROUND:
          return NotFeasible(session, event_args);
      }
      break;
    case STATE_WAITING_FOR_DELETION:
      switch (event) {
        case EVENT_RECOGNITION_ENDED:
          return SessionDelete(session, event_args);
        case EVENT_ABORT:
        case EVENT_START:
        case EVENT_STOP_CAPTURE:
        case EVENT_SET_BACKGROUND:
        case EVENT_RECOGNITION_RESULT:
        case EVENT_RECOGNITION_ERROR:
          return DoNothing(session, event_args);
      }
      break;
  }
  return NotFeasible(session, event_args);
}

// ----------- Contract for all the FSM evolution functions below -------------
//  - Are guaranteed to be executed in the IO thread;
//  - Are guaranteed to be not reentrant (themselves and each other);
//  - event_args members are guaranteed to be stable during the call;

SpeechRecognitionManagerImpl::FSMState
SpeechRecognitionManagerImpl::SessionStart(Session& session,
                                           const FSMEventArgs& event_args) {
  if (interactive_session_id_ != kSessionIDInvalid && delegate_)
    delegate_->DoClose(interactive_session_id_);
  interactive_session_id_ = session.id;
  if (delegate_)
    delegate_->ShowRecognitionRequested(session.id);
  session.recognizer->StartRecognition();
  return STATE_INTERACTIVE;
}

SpeechRecognitionManagerImpl::FSMState
SpeechRecognitionManagerImpl::SessionAbort(Session& session,
                                           const FSMEventArgs& event_args) {
  if (interactive_session_id_ == session.id) {
    interactive_session_id_ = kSessionIDInvalid;
    if (delegate_)
      delegate_->DoClose(session.id);
  }

  // If abort was requested while the recognizer was inactive, delete directly.
  if (session.recognizer == NULL || !session.recognizer->IsActive())
    return SessionDelete(session, event_args);

  // Otherwise issue an abort and delete gracefully, waiting for a
  // RECOGNITION_ENDED event first.
  session.recognizer->AbortRecognition();
  return STATE_WAITING_FOR_DELETION;
}

SpeechRecognitionManagerImpl::FSMState
SpeechRecognitionManagerImpl::SessionStopAudioCapture(
    Session& session, const FSMEventArgs& event_args) {
  DCHECK(session.recognizer != NULL);
  DCHECK(session.recognizer->IsActive());
  if (session.recognizer->IsCapturingAudio())
    session.recognizer->StopAudioCapture();
  return STATE_INTERACTIVE;
}

SpeechRecognitionManagerImpl::FSMState
SpeechRecognitionManagerImpl::SessionAbortIfCapturingAudioOrBackground(
    Session& session, const FSMEventArgs& event_args) {
  DCHECK_EQ(interactive_session_id_, session.id);

  DCHECK(session.recognizer != NULL);
  DCHECK(session.recognizer->IsActive());
  if (session.recognizer->IsCapturingAudio())
    return SessionAbort(session, event_args);

  interactive_session_id_ = kSessionIDInvalid;
  if (delegate_)
    delegate_->DoClose(session.id);
  return STATE_BACKGROUND;
}


SpeechRecognitionManagerImpl::FSMState
SpeechRecognitionManagerImpl::SessionSetBackground(
    Session& session, const FSMEventArgs& event_args) {
  DCHECK_EQ(interactive_session_id_, session.id);
  interactive_session_id_ = kSessionIDInvalid;
  if (delegate_)
    delegate_->DoClose(session.id);
  return STATE_BACKGROUND;
}

SpeechRecognitionManagerImpl::FSMState
SpeechRecognitionManagerImpl::SessionReportError(
    Session& session, const FSMEventArgs& event_args) {
  DCHECK_EQ(interactive_session_id_, session.id);
  if (delegate_)
    delegate_->ShowError(session.id, event_args.speech_error);
  return STATE_IDLE;
}

SpeechRecognitionManagerImpl::FSMState
SpeechRecognitionManagerImpl::SessionReportNoMatch(
    Session& session, const FSMEventArgs& event_args) {
  DCHECK_EQ(interactive_session_id_, session.id);
  if (delegate_) {
    delegate_->ShowError(
        session.id,
        SpeechRecognitionError(content::SPEECH_RECOGNITION_ERROR_NO_MATCH));
  }
  return STATE_IDLE;
}

SpeechRecognitionManagerImpl::FSMState
SpeechRecognitionManagerImpl::SessionDelete(Session& session,
                                            const FSMEventArgs& event_args) {
  DCHECK(session.recognizer == NULL || !session.recognizer->IsActive());
  if (interactive_session_id_ == session.id) {
    interactive_session_id_ = kSessionIDInvalid;
    if (delegate_)
      delegate_->DoClose(session.id);
  }
  sessions_.erase(session.id);
  // Next state is irrelevant, the session will be deleted afterwards.
  return STATE_WAITING_FOR_DELETION;
}

SpeechRecognitionManagerImpl::FSMState
SpeechRecognitionManagerImpl::DoNothing(Session& session,
                                        const FSMEventArgs& event_args) {
  return session.state;
}

SpeechRecognitionManagerImpl::FSMState
SpeechRecognitionManagerImpl::NotFeasible(Session& session,
                                          const FSMEventArgs& event_args) {
  NOTREACHED() << "Unfeasible event " << event_args.event
               << " in state " << session.state
               << " for session " << session.id;
  return session.state;
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

SpeechRecognitionEventListener* SpeechRecognitionManagerImpl::GetListener(
    int session_id) const {
  SessionsTable::const_iterator iter = sessions_.find(session_id);
  DCHECK(iter != sessions_.end());
  return iter->second.event_listener;
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

SpeechRecognitionManagerImpl::FSMEventArgs::FSMEventArgs(FSMEvent event_value)
  : event(event_value),
    speech_error(content::SPEECH_RECOGNITION_ERROR_NONE) {
}

SpeechRecognitionManagerImpl::FSMEventArgs::~FSMEventArgs() {
}

SpeechRecognitionManagerImpl::Session::Session()
  : id(kSessionIDInvalid),
    event_listener(NULL),
    state(STATE_IDLE) {
}

SpeechRecognitionManagerImpl::Session::~Session() {
}

}  // namespace speech
