// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_MANAGER_H_

#include "base/string16.h"
#include "content/common/content_export.h"

namespace content {

// This is the gatekeeper for speech recognition in the browser process. It
// handles requests received from various render views and makes sure only one
// of them can use speech recognition at a time. It also sends recognition
// results and status events to the render views when required.
class SpeechRecognitionManager {
 public:
  // Returns the singleton instance.
  CONTENT_EXPORT static SpeechRecognitionManager* GetInstance();

  // Starts/restarts recognition for an existing request.
  virtual void StartRecognitionForRequest(int session_id) = 0;

  // Cancels recognition for an existing request.
  virtual void CancelRecognitionForRequest(int session_id) = 0;

  // Called when the user clicks outside the speech input UI causing it to close
  // and possibly have speech input go to another element.
  virtual void FocusLostForRequest(int session_id) = 0;

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
