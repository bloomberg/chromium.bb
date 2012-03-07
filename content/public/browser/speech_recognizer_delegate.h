// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNIZER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNIZER_DELEGATE_H_
#pragma once

#include "content/public/common/speech_recognition_result.h"

namespace content {

// An interface to be implemented by consumers interested in receiving
// recognition events.
class SpeechRecognizerDelegate {
 public:
  virtual void SetRecognitionResult(int caller_id,
                                    const SpeechRecognitionResult& result) = 0;

  // Invoked when the first audio packet was received from the audio capture
  // device.
  virtual void DidStartReceivingAudio(int caller_id) = 0;

  // Invoked when audio recording stops, either due to the end pointer
  // detecting silence in user input or if |StopRecording| was called. The
  // delegate has to wait until |DidCompleteRecognition| is invoked before
  // destroying the |SpeechRecognizer| object.
  virtual void DidCompleteRecording(int caller_id) = 0;

  // This is guaranteed to be the last method invoked in the recognition
  // sequence and the |SpeechRecognizer| object can be freed up if necessary.
  virtual void DidCompleteRecognition(int caller_id) = 0;

  // Informs that the end pointer has started detecting speech.
  virtual void DidStartReceivingSpeech(int caller_id) = 0;

  // Informs that the end pointer has stopped detecting speech.
  virtual void DidStopReceivingSpeech(int caller_id) = 0;

  // Invoked if there was an error while recording or recognizing audio. The
  // session has already been cancelled when this call is made and the DidXxxx
  // callbacks will not be issued. It is safe to destroy/release the
  // |SpeechRecognizer| object while processing this call.
  virtual void OnRecognizerError(int caller_id,
                                 SpeechRecognitionErrorCode error) = 0;

  // At the start of recognition, a short amount of audio is recorded to
  // estimate the environment/background noise and this callback is issued
  // after that is complete. Typically the delegate brings up any speech
  // recognition UI once this callback is received.
  virtual void DidCompleteEnvironmentEstimation(int caller_id) = 0;

  // Informs of a change in the captured audio level, useful if displaying
  // a microphone volume indicator while recording.
  // The value of |volume| and |noise_volume| is in the [0.0, 1.0] range.
  virtual void SetInputVolume(int caller_id,
                              float volume,
                              float noise_volume) = 0;

 protected:
  virtual ~SpeechRecognizerDelegate() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNIZER_DELEGATE_H_
