// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_MANAGER_IMPL_H_
#define CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_MANAGER_IMPL_H_
#pragma once

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "content/public/browser/speech_recognition_manager.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "content/public/browser/speech_recognition_session_context.h"
#include "content/public/common/speech_recognition_error.h"

namespace content {
class BrowserMainLoop;
class SpeechRecognitionManagerDelegate;
}

namespace speech {

class SpeechRecognizerImpl;

// This is the manager for speech recognition. It is a single instance in
// the browser process and can serve several requests. Each recognition request
// corresponds to a session, initiated via |CreateSession|.
// In every moment the manager has at most one session capturing audio, which
// is identified by |session_id_capturing_audio_|. However, multiple sessions
// can live in parallel in respect of the aforementioned constraint, i.e. while
// waiting for results.
// This class does not handle user interface objects (bubbles, tray icon).
// Those are managed by the delegate, which receives a copy of all sessions
// events.
//
// The SpeechRecognitionManager has the following responsibilities:
//  - Handles requests received from various render views and makes sure only
//    one of them accesses the audio device at any given time.
//  - Handles the instantiation of SpeechRecognitionEngine objects when
//    requested by SpeechRecognitionSessions.
//  - Relays recognition results/status/error events of each session to the
//    corresponding listener (demuxing on the base of their session_id).
//  - Relays also recognition results/status/error events of every session to
//    the catch-all snoop listener (optionally) provided by the delegate.
class CONTENT_EXPORT SpeechRecognitionManagerImpl :
    public NON_EXPORTED_BASE(content::SpeechRecognitionManager),
    public content::SpeechRecognitionEventListener {
 public:
  // Returns the current SpeechRecognitionManagerImpl. Can be called only after
  // the RecognitionMnager has been created (by BrowserMainLoop).
  static SpeechRecognitionManagerImpl* GetInstance();

  // SpeechRecognitionManager implementation.
  virtual int CreateSession(
      const content::SpeechRecognitionSessionConfig& config) OVERRIDE;
  virtual void StartSession(int session_id) OVERRIDE;
  virtual void AbortSession(int session_id) OVERRIDE;
  virtual void AbortAllSessionsForListener(
        content::SpeechRecognitionEventListener* listener) OVERRIDE;
  virtual void StopAudioCaptureForSession(int session_id) OVERRIDE;
  virtual const content::SpeechRecognitionSessionConfig& GetSessionConfig(
      int session_id) const OVERRIDE;
  virtual content::SpeechRecognitionSessionContext GetSessionContext(
      int session_id) const OVERRIDE;
  virtual int LookupSessionByContext(
      base::Callback<bool(
          const content::SpeechRecognitionSessionContext&)> matcher)
            const OVERRIDE;
  virtual bool HasAudioInputDevices() OVERRIDE;
  virtual bool IsCapturingAudio() OVERRIDE;
  virtual string16 GetAudioInputDeviceModel() OVERRIDE;
  virtual void ShowAudioInputSettings() OVERRIDE;

  // SpeechRecognitionEventListener methods.
  virtual void OnRecognitionStart(int session_id) OVERRIDE;
  virtual void OnAudioStart(int session_id) OVERRIDE;
  virtual void OnEnvironmentEstimationComplete(int session_id) OVERRIDE;
  virtual void OnSoundStart(int session_id) OVERRIDE;
  virtual void OnSoundEnd(int session_id) OVERRIDE;
  virtual void OnAudioEnd(int session_id) OVERRIDE;
  virtual void OnRecognitionEnd(int session_id) OVERRIDE;
  virtual void OnRecognitionResult(
      int session_id, const content::SpeechRecognitionResult& result) OVERRIDE;
  virtual void OnRecognitionError(
      int session_id, const content::SpeechRecognitionError& error) OVERRIDE;
  virtual void OnAudioLevelsChange(int session_id, float volume,
                                   float noise_volume) OVERRIDE;

 protected:
  // BrowserMainLoop is the only one allowed to istantiate and free us.
  friend class content::BrowserMainLoop;
  friend class scoped_ptr<SpeechRecognitionManagerImpl>;  // Needed for dtor.
  SpeechRecognitionManagerImpl();
  virtual ~SpeechRecognitionManagerImpl();

 private:
  // Data types for the internal Finite State Machine (FSM).
  enum FSMState {
    SESSION_STATE_IDLE = 0,
    SESSION_STATE_CAPTURING_AUDIO,
    SESSION_STATE_WAITING_FOR_RESULT,
    SESSION_STATE_MAX_VALUE = SESSION_STATE_WAITING_FOR_RESULT
  };

  enum FSMEvent {
    EVENT_ABORT = 0,
    EVENT_START,
    EVENT_STOP_CAPTURE,
    EVENT_AUDIO_ENDED,
    EVENT_RECOGNITION_ENDED,
    EVENT_MAX_VALUE = EVENT_RECOGNITION_ENDED
  };

  struct Session {
    Session();
    ~Session();

    int id;
    bool listener_is_active;
    content::SpeechRecognitionSessionConfig config;
    content::SpeechRecognitionSessionContext context;
    scoped_refptr<SpeechRecognizerImpl> recognizer;
  };

  // Callback issued by the SpeechRecognitionManagerDelegate for reporting
  // asynchronously the result of the CheckRecognitionIsAllowed call.
  void RecognitionAllowedCallback(int session_id, bool is_allowed);

  // Entry point for pushing any external event into the session handling FSM.
  void DispatchEvent(int session_id, FSMEvent event);

  // Defines the behavior of the session handling FSM, selecting the appropriate
  // transition according to the session, its current state and the event.
  void ExecuteTransitionAndGetNextState(
      const Session& session, FSMState session_state, FSMEvent event);

  // Retrieves the state of the session, enquiring directly the recognizer.
  FSMState GetSessionState(int session_id) const;

  // The methods below handle transitions of the session handling FSM.
  void SessionStart(const Session& session);
  void SessionAbort(const Session& session);
  void SessionStopAudioCapture(const Session& session);
  void ResetCapturingSessionId(const Session& session);
  void SessionDelete(const Session& session);
  void NotFeasible(const Session& session, FSMEvent event);

  bool SessionExists(int session_id) const;
  const Session& GetSession(int session_id) const;
  content::SpeechRecognitionEventListener* GetListener(int session_id) const;
  content::SpeechRecognitionEventListener* GetDelegateListener() const;
  int GetNextSessionID();

  typedef std::map<int, Session> SessionsTable;
  SessionsTable sessions_;
  int session_id_capturing_audio_;
  int last_session_id_;
  bool is_dispatching_event_;
  scoped_ptr<content::SpeechRecognitionManagerDelegate> delegate_;
};

}  // namespace speech

#endif  // CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_MANAGER_IMPL_H_
