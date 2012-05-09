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
#include "base/memory/singleton.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "content/public/browser/speech_recognition_manager.h"
#include "content/public/browser/speech_recognition_session_context.h"
#include "content/public/common/speech_recognition_error.h"

namespace content {
class SpeechRecognitionManagerDelegate;
}

namespace speech {

class SpeechRecognizerImpl;

// This is the manager for speech recognition. It is a singleton instance in
// the browser process and can serve several requests. Each recognition request
// corresponds to a session, initiated via |CreateSession|.
// In every moment the manager has at most one "interactive" session (identified
// by |interactive_session_id_|), that is the session that is currently holding
// user attention. For privacy reasons, only the interactive session is allowed
// to capture audio from the microphone. However, after audio capture is
// completed, a session can be sent to background and can live in parallel with
// other sessions, while waiting for its results.
//
// More in details, SpeechRecognitionManager has the following responsibilities:
//  - Handles requests received from various render views and makes sure only
//    one of them accesses the audio device at any given time.
//  - Relays recognition results/status/error events of each session to the
//    corresponding listener (demuxing on the base of their session_id).
//  - Handles the instantiation of SpeechRecognitionEngine objects when
//    requested by SpeechRecognitionSessions.
class CONTENT_EXPORT SpeechRecognitionManagerImpl :
    public NON_EXPORTED_BASE(content::SpeechRecognitionManager),
    public NON_EXPORTED_BASE(content::SpeechRecognitionEventListener) {
 public:
  static SpeechRecognitionManagerImpl* GetInstance();

  // SpeechRecognitionManager implementation.
  virtual int CreateSession(
      const content::SpeechRecognitionSessionConfig& config,
      SpeechRecognitionEventListener* event_listener) OVERRIDE;
  virtual void StartSession(int session_id) OVERRIDE;
  virtual void AbortSession(int session_id) OVERRIDE;
  virtual void AbortAllSessionsForListener(
        content::SpeechRecognitionEventListener* listener) OVERRIDE;
  virtual void StopAudioCaptureForSession(int session_id) OVERRIDE;
  virtual void SendSessionToBackground(int session_id) OVERRIDE;
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
  // Private constructor to enforce singleton.
  friend struct DefaultSingletonTraits<SpeechRecognitionManagerImpl>;
  SpeechRecognitionManagerImpl();
  virtual ~SpeechRecognitionManagerImpl();

 private:
  // Data types for the internal Finite State Machine (FSM).
  enum FSMState {
    STATE_IDLE = 0,
    STATE_INTERACTIVE,
    STATE_BACKGROUND,
    STATE_WAITING_FOR_DELETION,
    STATE_MAX_VALUE = STATE_WAITING_FOR_DELETION
  };

  enum FSMEvent {
    EVENT_ABORT = 0,
    EVENT_START,
    EVENT_STOP_CAPTURE,
    EVENT_SET_BACKGROUND,
    EVENT_RECOGNITION_ENDED,
    EVENT_RECOGNITION_RESULT,
    EVENT_RECOGNITION_ERROR,
    EVENT_MAX_VALUE = EVENT_RECOGNITION_ERROR
  };

  struct Session {
    Session();
    ~Session();

    int id;
    content::SpeechRecognitionEventListener* event_listener;
    content::SpeechRecognitionSessionContext context;
    scoped_refptr<SpeechRecognizerImpl> recognizer;
    FSMState state;
    bool error_occurred;
  };

  struct FSMEventArgs {
    explicit FSMEventArgs(FSMEvent event_value);
    ~FSMEventArgs();

    FSMEvent event;
    content::SpeechRecognitionError speech_error;
  };

  // Callback issued by the SpeechRecognitionManagerDelegate for reporting
  // asynchronously the result of the CheckRecognitionIsAllowed call.
  void RecognitionAllowedCallback(int session_id, bool is_allowed);

  // Entry point for pushing any external event into the session handling FSM.
  void DispatchEvent(int session_id, FSMEventArgs args);

  // Defines the behavior of the session handling FSM, selecting the appropriate
  // transition according to the session, its current state and the event.
  FSMState ExecuteTransitionAndGetNextState(Session& session,
                                            const FSMEventArgs& event_args);

  // The methods below handle transitions of the session handling FSM.
  FSMState SessionStart(Session& session, const FSMEventArgs& event_args);
  FSMState SessionAbort(Session& session, const FSMEventArgs& event_args);
  FSMState SessionStopAudioCapture(Session& session,
                                   const FSMEventArgs& event_args);
  FSMState SessionAbortIfCapturingAudioOrBackground(
      Session& session, const FSMEventArgs& event_args);
  FSMState SessionSetBackground(Session& session,
                                const FSMEventArgs& event_args);
  FSMState SessionReportError(Session& session, const FSMEventArgs& event_args);
  FSMState SessionReportNoMatch(Session& session,
                                const FSMEventArgs& event_args);
  FSMState SessionDelete(Session& session, const FSMEventArgs& event_args);
  FSMState DoNothing(Session& session, const FSMEventArgs& event_args);
  FSMState NotFeasible(Session& session, const FSMEventArgs& event_args);

  bool SessionExists(int session_id) const;
  content::SpeechRecognitionEventListener* GetListener(int session_id) const;
  int GetNextSessionID();

  typedef std::map<int, Session> SessionsTable;
  SessionsTable sessions_;
  int interactive_session_id_;
  int last_session_id_;
  bool is_dispatching_event_;
  content::SpeechRecognitionManagerDelegate* delegate_;
};

}  // namespace speech

#endif  // CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_MANAGER_IMPL_H_
