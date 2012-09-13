// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/speech_recognition_manager_impl.h"

#include "base/bind.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/speech/google_one_shot_remote_engine.h"
#include "content/browser/speech/google_streaming_remote_engine.h"
#include "content/browser/speech/speech_recognition_engine.h"
#include "content/browser/speech/speech_recognizer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "content/public/browser/speech_recognition_manager_delegate.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "content/public/browser/speech_recognition_session_context.h"
#include "content/public/common/speech_recognition_error.h"
#include "content/public/common/speech_recognition_result.h"
#include "media/audio/audio_manager.h"

using base::Callback;
using content::BrowserMainLoop;
using content::BrowserThread;
using content::SpeechRecognitionEventListener;
using content::SpeechRecognitionManager;
using content::SpeechRecognitionResult;
using content::SpeechRecognitionSessionContext;
using content::SpeechRecognitionSessionConfig;

namespace content {
SpeechRecognitionManager* SpeechRecognitionManager::GetInstance() {
  return speech::SpeechRecognitionManagerImpl::GetInstance();
}
}  // namespace content

namespace {
speech::SpeechRecognitionManagerImpl* g_speech_recognition_manager_impl;

void ShowAudioInputSettingsOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  media::AudioManager* audio_manager = BrowserMainLoop::GetAudioManager();
  DCHECK(audio_manager->CanShowAudioInputSettings());
  if (audio_manager->CanShowAudioInputSettings())
    audio_manager->ShowAudioInputSettings();
}
}  // namespace

namespace speech {

class SpeechRecognitionManagerImpl::PermissionRequest
    : public media_stream::MediaStreamRequester {
 public:
  PermissionRequest(int session_id,
                    const base::Callback<void(bool is_allowed)>& callback)
      : session_id_(session_id),
        callback_(callback),
        started_(false) {
  }

  virtual ~PermissionRequest() {
    if (started_)
      Abort();
  }

  void Start(int render_process_id, int render_view_id, const GURL& origin) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    started_ = true;
    BrowserMainLoop::GetMediaStreamManager()->GenerateStream(
        this,
        render_process_id,
        render_view_id,
        media_stream::StreamOptions(/*audio=*/true, /*video=*/false),
        origin,
        &label_);
  }

  void Abort() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    started_ = false;
    BrowserMainLoop::GetMediaStreamManager()->CancelGenerateStream(label_);
  }

  int Session() const { return session_id_; }

  // MediaStreamRequester methods.
  virtual void StreamGenerated(
      const std::string& label,
      const media_stream::StreamDeviceInfoArray& audio_devices,
      const media_stream::StreamDeviceInfoArray& video_devices) OVERRIDE {
    // TODO(hans): One day it would be nice to actually use the generated stream
    // but right now we only use it to request permission, and then we dump it.
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    started_ = false;
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&media_stream::MediaStreamManager::StopGeneratedStream,
                   base::Unretained(BrowserMainLoop::GetMediaStreamManager()),
                   label));
    callback_.Run(true);
  }

  virtual void StreamGenerationFailed(const std::string& label) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    started_ = false;
    callback_.Run(false);
  }

  // The callbacks below are ignored.
  virtual void AudioDeviceFailed(const std::string& label,
                                 int index) OVERRIDE {}
  virtual void VideoDeviceFailed(const std::string& label,
                                 int index) OVERRIDE {}
  virtual void DevicesEnumerated(
      const std::string& label,
      const media_stream::StreamDeviceInfoArray& devices) OVERRIDE {}
  virtual void DeviceOpened(
      const std::string& label,
      const media_stream::StreamDeviceInfo& device_info) OVERRIDE {}

 private:
  int session_id_;
  base::Callback<void(bool is_allowed)> callback_;
  std::string label_;
  bool started_;
};

SpeechRecognitionManagerImpl* SpeechRecognitionManagerImpl::GetInstance() {
  return g_speech_recognition_manager_impl;
}

SpeechRecognitionManagerImpl::SpeechRecognitionManagerImpl()
    : primary_session_id_(kSessionIDInvalid),
      last_session_id_(kSessionIDInvalid),
      is_dispatching_event_(false),
      delegate_(content::GetContentClient()->browser()->
                    GetSpeechRecognitionManagerDelegate()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
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
  remote_engine_config.audio_sample_rate = SpeechRecognizer::kAudioSampleRate;
  remote_engine_config.audio_num_bits_per_sample =
     SpeechRecognizer::kNumBitsPerAudioSample;
  remote_engine_config.filter_profanities = config.filter_profanities;
  remote_engine_config.max_hypotheses = config.max_hypotheses;
  remote_engine_config.hardware_info = hardware_info;
  remote_engine_config.origin_url = can_report_metrics ? config.origin_url : "";

  SpeechRecognitionEngine* google_remote_engine;
  if (config.is_one_shot) {
    google_remote_engine =
        new GoogleOneShotRemoteEngine(config.url_request_context_getter);
  } else {
    google_remote_engine =
        new GoogleStreamingRemoteEngine(config.url_request_context_getter);
  }

  google_remote_engine->SetConfig(remote_engine_config);

  session.recognizer = new SpeechRecognizer(this,
                                            session_id,
                                            config.is_one_shot,
                                            google_remote_engine);
  return session_id;
}

void SpeechRecognitionManagerImpl::StartSession(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  // If there is another active session, abort that.
  if (primary_session_id_ != kSessionIDInvalid &&
      primary_session_id_ != session_id) {
    AbortSession(primary_session_id_);
  }

  primary_session_id_ = session_id;

  if (delegate_.get()) {
    delegate_->CheckRecognitionIsAllowed(
        session_id,
        base::Bind(&SpeechRecognitionManagerImpl::RecognitionAllowedCallback,
                   weak_factory_.GetWeakPtr(),
                   session_id));
  }
}

void SpeechRecognitionManagerImpl::RecognitionAllowedCallback(int session_id,
                                                              bool ask_user,
                                                              bool is_allowed) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  if (ask_user) {
    const SpeechRecognitionSessionContext& context =
        GetSessionContext(session_id);

    permission_request_.reset(new PermissionRequest(
        session_id,
        base::Bind(&SpeechRecognitionManagerImpl::RecognitionAllowedCallback,
                   weak_factory_.GetWeakPtr(),
                   session_id,
                   false)));

    permission_request_->Start(context.render_process_id,
                               context.render_view_id,
                               GURL(context.context_name));

    return;
  }

  permission_request_.reset();

  if (is_allowed) {
    MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(&SpeechRecognitionManagerImpl::DispatchEvent,
                   weak_factory_.GetWeakPtr(), session_id, EVENT_START));
  } else {
    OnRecognitionError(session_id, content::SpeechRecognitionError(
        content::SPEECH_RECOGNITION_ERROR_NOT_ALLOWED));
    MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(&SpeechRecognitionManagerImpl::DispatchEvent,
                   weak_factory_.GetWeakPtr(), session_id, EVENT_ABORT));
  }
}

void SpeechRecognitionManagerImpl::AbortSession(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  if (permission_request_.get() &&
      permission_request_->Session() == session_id) {
    DCHECK(permission_request_.get());
    permission_request_->Abort();
  }

  MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&SpeechRecognitionManagerImpl::DispatchEvent,
                 weak_factory_.GetWeakPtr(), session_id, EVENT_ABORT));
}

void SpeechRecognitionManagerImpl::StopAudioCaptureForSession(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  if (permission_request_.get() &&
      permission_request_->Session() == session_id) {
    DCHECK(permission_request_.get());
    permission_request_->Abort();
  }

  MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&SpeechRecognitionManagerImpl::DispatchEvent,
                 weak_factory_.GetWeakPtr(), session_id, EVENT_STOP_CAPTURE));
}

// Here begins the SpeechRecognitionEventListener interface implementation,
// which will simply relay the events to the proper listener registered for the
// particular session (most likely InputTagSpeechDispatcherHost) and to the
// catch-all listener provided by the delegate (if any).

void SpeechRecognitionManagerImpl::OnRecognitionStart(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  DCHECK_EQ(primary_session_id_, session_id);
  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnRecognitionStart(session_id);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnRecognitionStart(session_id);
}

void SpeechRecognitionManagerImpl::OnAudioStart(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  DCHECK_EQ(primary_session_id_, session_id);
  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnEnvironmentEstimationComplete(session_id);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnEnvironmentEstimationComplete(session_id);
}

void SpeechRecognitionManagerImpl::OnSoundStart(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!SessionExists(session_id))
    return;

  DCHECK_EQ(primary_session_id_, session_id);
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
  MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&SpeechRecognitionManagerImpl::DispatchEvent,
                 weak_factory_.GetWeakPtr(), session_id, EVENT_AUDIO_ENDED));
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
  MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&SpeechRecognitionManagerImpl::DispatchEvent,
                 weak_factory_.GetWeakPtr(),
                 session_id,
                 EVENT_RECOGNITION_ENDED));
}

int SpeechRecognitionManagerImpl::GetSession(
    int render_process_id, int render_view_id, int request_id) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  SessionsTable::const_iterator iter;
  for(iter = sessions_.begin(); iter != sessions_.end(); ++iter) {
    const int session_id = iter->first;
    const SpeechRecognitionSessionContext& context = iter->second.context;
    if (context.render_process_id == render_process_id &&
        context.render_view_id == render_view_id &&
        context.request_id == request_id) {
      return session_id;
    }
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

void SpeechRecognitionManagerImpl::AbortAllSessionsForRenderView(
    int render_process_id,
    int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  for (SessionsTable::iterator it = sessions_.begin(); it != sessions_.end();
       ++it) {
    Session& session = it->second;
    if (session.context.render_process_id == render_process_id &&
        session.context.render_view_id == render_view_id) {
      AbortSession(session.id);
    }
  }
}

// -----------------------  Core FSM implementation ---------------------------
void SpeechRecognitionManagerImpl::DispatchEvent(int session_id,
                                                 FSMEvent event) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // There are some corner cases in which the session might be deleted (due to
  // an EndRecognition event) between a request (e.g. Abort) and its dispatch.
  if (!SessionExists(session_id))
    return;

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
// All the events received by the SpeechRecognizer instances (one for each
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
          return SessionAbort(session);
        case EVENT_RECOGNITION_ENDED:
          return SessionDelete(session);
        case EVENT_STOP_CAPTURE:
          return SessionStopAudioCapture(session);
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
  DCHECK_EQ(primary_session_id_, session.id);
  session.recognizer->StartRecognition();
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

void SpeechRecognitionManagerImpl::SessionDelete(const Session& session) {
  DCHECK(session.recognizer == NULL || !session.recognizer->IsActive());
  if (primary_session_id_ == session.id)
    primary_session_id_ = kSessionIDInvalid;
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
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&ShowAudioInputSettingsOnFileThread));
}

SpeechRecognitionManagerImpl::Session::Session()
  : id(kSessionIDInvalid),
    listener_is_active(true) {
}

SpeechRecognitionManagerImpl::Session::~Session() {
}

}  // namespace speech
