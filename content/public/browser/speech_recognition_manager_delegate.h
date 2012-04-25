// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
#pragma once

#include <string>

#include "base/callback_forward.h"
#include "content/public/common/speech_recognition_error.h"

namespace content {

struct SpeechRecognitionResult;

// Allows embedders to display the current state of recognition, for getting the
// user's permission and for fetching optional request information.
class SpeechRecognitionManagerDelegate {
 public:
  virtual ~SpeechRecognitionManagerDelegate() {}

  // Get the optional diagnostic hardware information if available.
  virtual void GetDiagnosticInformation(bool* can_report_metrics,
                                        std::string* hardware_info) = 0;

  // Called when recognition has been requested. The source point of the view
  // port can be retrieved looking-up the session context.
  virtual void ShowRecognitionRequested(int session_id) = 0;

  // Checks (asynchronously) if current setup allows speech recognition.
  virtual void CheckRecognitionIsAllowed(
      int session_id,
      base::Callback<void(int session_id, bool is_allowed)> callback) = 0;

  // Called when recognition is starting up.
  virtual void ShowWarmUp(int session_id) = 0;

  // Called when recognition has started.
  virtual void ShowRecognizing(int session_id) = 0;

  // Called when recording has started.
  virtual void ShowRecording(int session_id) = 0;

  // Continuously updated with the current input volume.
  virtual void ShowInputVolume(int session_id,
                               float volume,
                               float noise_volume) = 0;

  // Called when there has been a error with the recognition.
  virtual void ShowError(int session_id,
                         const SpeechRecognitionError& error) = 0;

  // Called when recognition has ended or has been canceled.
  virtual void DoClose(int session_id) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
