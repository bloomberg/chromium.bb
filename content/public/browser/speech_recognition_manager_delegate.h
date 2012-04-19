// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
#pragma once

#include <string>

#include "content/public/common/speech_recognition_error.h"

namespace gfx {
class Rect;
}

namespace content {

struct SpeechRecognitionResult;

// Allows embedders to display the current state of recognition, for getting the
// user's permission and for fetching optional request information.
class SpeechRecognitionManagerDelegate {
 public:
  // Describes the microphone errors that are reported via ShowMicError.
  enum MicError {
    MIC_ERROR_NO_DEVICE_AVAILABLE = 0,
    MIC_ERROR_DEVICE_IN_USE
  };

  virtual ~SpeechRecognitionManagerDelegate() {}

  // Get the optional request information if available.
  virtual void GetRequestInfo(bool* can_report_metrics,
                              std::string* request_info) = 0;

  // Called when recognition has been requested from point |element_rect_| on
  // the view port for the given caller. The embedder should call the
  // StartRecognition or CancelRecognition methods on SpeechInutManager in
  // response.
  virtual void ShowRecognitionRequested(int session_id,
                                        int render_process_id,
                                        int render_view_id,
                                        const gfx::Rect& element_rect) = 0;

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

  // Called when no microphone has been found.
  virtual void ShowMicError(int session_id, MicError error) = 0;

  // Called when there has been a error with the recognition.
  virtual void ShowRecognizerError(int session_id,
                                   SpeechRecognitionErrorCode error) = 0;

  // Called when recognition has ended or has been canceled.
  virtual void DoClose(int session_id) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
