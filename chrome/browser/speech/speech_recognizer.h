// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_RECOGNIZER_H_
#define CHROME_BROWSER_SPEECH_SPEECH_RECOGNIZER_H_

#include <list>
#include <string>
#include <utility>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/speech/endpointer/endpointer.h"
#include "chrome/browser/speech/speech_recognition_request.h"
#include "media/audio/audio_input_controller.h"

namespace speech_input {

class SpeexEncoder;

// Records audio, sends recorded audio to server and translates server response
// to recognition result.
class SpeechRecognizer
    : public base::RefCountedThreadSafe<SpeechRecognizer>,
      public media::AudioInputController::EventHandler,
      public SpeechRecognitionRequestDelegate {
 public:
  enum ErrorCode {
    RECOGNIZER_NO_ERROR,
    RECOGNIZER_ERROR_CAPTURE,
    RECOGNIZER_ERROR_NO_SPEECH,
    RECOGNIZER_ERROR_NO_RESULTS,
  };

  // Implemented by the caller to receive recognition events.
  class Delegate {
   public:
    virtual void SetRecognitionResult(int caller_id,
                                      bool error,
                                      const string16& value) = 0;

    // Invoked when audio recording stops, either due to the end pointer
    // detecting silence in user input or if |StopRecording| was called. The
    // delegate has to wait until |DidCompleteRecognition| is invoked before
    // destroying the |SpeechRecognizer| object.
    virtual void DidCompleteRecording(int caller_id) = 0;

    // This is guaranteed to be the last method invoked in the recognition
    // sequence and the |SpeechRecognizer| object can be freed up if necessary.
    virtual void DidCompleteRecognition(int caller_id) = 0;

    // Invoked if there was an error while recording or recognizing audio. The
    // session is terminated when this call is made and the DidXxxx callbacks
    // are issued after this call.
    virtual void OnRecognizerError(int caller_id,
                                   SpeechRecognizer::ErrorCode error) = 0;

    // At the start of recognition, a short amount of audio is recorded to
    // estimate the environment/background noise and this callback is issued
    // after that is complete. Typically the delegate brings up any speech
    // recognition UI once this callback is received.
    virtual void DidCompleteEnvironmentEstimation(int caller_id) = 0;

   protected:
    virtual ~Delegate() {}
  };

  SpeechRecognizer(Delegate* delegate, int caller_id);
  ~SpeechRecognizer();

  // Starts audio recording and does recognition after recording ends. The same
  // SpeechRecognizer instance can be used multiple times for speech recognition
  // though each recognition request can be made only after the previous one
  // completes (i.e. after receiving Delegate::DidCompleteRecognition).
  bool StartRecording();

  // Stops recording audio and starts recognition.
  void StopRecording();

  // Stops recording audio and cancels recognition. Any audio recorded so far
  // gets discarded.
  void CancelRecognition();

  // AudioInputController::EventHandler methods.
  void OnCreated(media::AudioInputController* controller) { }
  void OnRecording(media::AudioInputController* controller) { }
  void OnError(media::AudioInputController* controller, int error_code);
  void OnData(media::AudioInputController* controller, const uint8* data,
              uint32 size);

  // SpeechRecognitionRequest::Delegate methods.
  void SetRecognitionResult(bool error, const string16& value);

  static const int kAudioSampleRate;
  static const int kAudioPacketIntervalMs;  // Duration of each audio packet.
  static const int kNumAudioChannels;
  static const int kNumBitsPerAudioSample;
  static const int kNoSpeechTimeoutSec;

 private:
  void ReleaseAudioBuffers();
  void InformErrorAndCancelRecognition(ErrorCode error);

  void HandleOnError(int error_code);  // Handles OnError in the IO thread.

  // Handles OnData in the IO thread. Takes ownership of |data|.
  void HandleOnData(std::string* data);

  Delegate* delegate_;
  int caller_id_;

  // Buffer holding the recorded audio. Owns the strings inside the list.
  typedef std::list<std::string*> AudioBufferQueue;
  AudioBufferQueue audio_buffers_;

  scoped_ptr<SpeechRecognitionRequest> request_;
  scoped_refptr<media::AudioInputController> audio_controller_;
  scoped_ptr<SpeexEncoder> encoder_;
  Endpointer endpointer_;
  int num_samples_recorded_;

  DISALLOW_COPY_AND_ASSIGN(SpeechRecognizer);
};

// This typedef is to workaround the issue with certain versions of
// Visual Studio where it gets confused between multiple Delegate
// classes and gives a C2500 error. (I saw this error on the try bots -
// the workaround was not needed for my machine).
typedef SpeechRecognizer::Delegate SpeechRecognizerDelegate;

}  // namespace speech_input

#endif  // CHROME_BROWSER_SPEECH_SPEECH_RECOGNIZER_H_
