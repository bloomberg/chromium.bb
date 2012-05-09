// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_MANAGER_H_
#pragma once

#include "base/string16.h"
#include "base/callback.h"
#include "content/common/content_export.h"
#include "content/public/common/speech_recognition_result.h"

namespace content {

class SpeechRecognitionEventListener;
struct SpeechRecognitionSessionConfig;
struct SpeechRecognitionSessionContext;

// The SpeechRecognitionManager (SRM) is a singleton class that handles SR
// functionalities within Chrome. Everyone that needs to perform SR should
// interface exclusively with the SRM, receiving events through the callback
// interface SpeechRecognitionEventListener.
// Since many different sources can use SR in different times (some overlapping
// is allowed while waiting for results), the SRM has the further responsibility
// of handling separately and reliably (taking into account also call sequences
// that might not make sense, e.g., two subsequent AbortSession calls).
// In this sense a session, within the SRM, models the ongoing evolution of a
// SR request from the viewpoint of the end-user, abstracting all the concrete
// operations that must be carried out, that will be handled by inner classes.
class SpeechRecognitionManager {
 public:
  static const int kSessionIDInvalid;

  // Returns the singleton instance.
  static CONTENT_EXPORT SpeechRecognitionManager* GetInstance();

  // Creates a new recognition session.
  virtual int CreateSession(const SpeechRecognitionSessionConfig& config,
                            SpeechRecognitionEventListener* listener) = 0;

  // Starts/restarts recognition for an existing session, after performing a
  // premilinary check on the delegate (CheckRecognitionIsAllowed).
  virtual void StartSession(int session_id) = 0;

  // Aborts recognition for an existing session, without providing any result.
  virtual void AbortSession(int session_id) = 0;

  // Aborts all sessions for a given listener, without providing any result.
  virtual void AbortAllSessionsForListener(
      SpeechRecognitionEventListener* listener) = 0;

  // Stops audio capture for an existing session. The audio captured before the
  // call will be processed, possibly ending up with a result.
  virtual void StopAudioCaptureForSession(int session_id) = 0;

  // Sends the session to background preventing it from further interacting with
  // the browser (typically invoked when the user clicks outside the speech UI).
  // The session will be silently continued in background if possible (in case
  // it already finished capturing audio and was just waiting for the result) or
  // will be aborted if user interaction (e.g., audio recording) was involved
  // when this function was called.
  virtual void SendSessionToBackground(int session_id) = 0;

  // Retrieves the context associated to a session.
  virtual SpeechRecognitionSessionContext GetSessionContext(
      int session_id) const = 0;

  // Looks-up an existing session using a caller-provided matcher function.
  virtual int LookupSessionByContext(
      base::Callback<bool(
          const content::SpeechRecognitionSessionContext&)> matcher)
            const = 0;

  // Returns true if the OS reports existence of audio recording devices.
  virtual bool HasAudioInputDevices() = 0;

  // Used to determine if something else is currently making use of audio input.
  virtual bool IsCapturingAudio() = 0;

  // Returns a human readable string for the model/make of the active audio
  // input device for this computer.
  virtual string16 GetAudioInputDeviceModel() = 0;

  // Invokes the platform provided microphone settings UI in a non-blocking way,
  // via the BrowserThread::FILE thread.
  virtual void ShowAudioInputSettings() = 0;

 protected:
   virtual ~SpeechRecognitionManager() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_MANAGER_H_
