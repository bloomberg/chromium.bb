// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_SPEECH_RECOGNIZER_H_
#define CONTENT_BROWSER_SPEECH_SPEECH_RECOGNIZER_H_

#include <list>
#include <string>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/speech/audio_encoder.h"
#include "content/browser/speech/endpointer/endpointer.h"
#include "content/browser/speech/speech_recognition_request.h"
#include "content/common/content_export.h"
#include "media/audio/audio_input_controller.h"

namespace net {
class URLRequestContextGetter;
}

namespace speech_input {

// Records audio, sends recorded audio to server and translates server response
// to recognition result.
class CONTENT_EXPORT SpeechRecognizer
    : public base::RefCountedThreadSafe<SpeechRecognizer>,
      public media::AudioInputController::EventHandler,
      public SpeechRecognitionRequestDelegate {
 public:
  // Implemented by the caller to receive recognition events.
  class CONTENT_EXPORT Delegate {
   public:
    virtual void SetRecognitionResult(
        int caller_id,
        const SpeechInputResult& result) = 0;

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
                                   SpeechInputError error) = 0;

    // At the start of recognition, a short amount of audio is recorded to
    // estimate the environment/background noise and this callback is issued
    // after that is complete. Typically the delegate brings up any speech
    // recognition UI once this callback is received.
    virtual void DidCompleteEnvironmentEstimation(int caller_id) = 0;

    // Informs of a change in the captured audio level, useful if displaying
    // a microphone volume indicator while recording.
    // The value of |volume| and |noise_volume| is in the [0.0, 1.0] range.
    virtual void SetInputVolume(int caller_id, float volume,
                                float noise_volume) = 0;

   protected:
    virtual ~Delegate() {}
  };

  SpeechRecognizer(Delegate* delegate,
                   int caller_id,
                   const std::string& language,
                   const std::string& grammar,
                   net::URLRequestContextGetter* context_getter,
                   bool filter_profanities,
                   const std::string& hardware_info,
                   const std::string& origin_url);

  virtual ~SpeechRecognizer();

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
  virtual void OnCreated(media::AudioInputController* controller) OVERRIDE { }
  virtual void OnRecording(media::AudioInputController* controller) OVERRIDE { }
  virtual void OnError(media::AudioInputController* controller,
                       int error_code) OVERRIDE;
  virtual void OnData(media::AudioInputController* controller,
                      const uint8* data,
                      uint32 size) OVERRIDE;

  // SpeechRecognitionRequest::Delegate methods.
  virtual void SetRecognitionResult(const SpeechInputResult& result) OVERRIDE;

  static const int kAudioSampleRate;
  static const int kAudioPacketIntervalMs;  // Duration of each audio packet.
  static const ChannelLayout kChannelLayout;
  static const int kNumBitsPerAudioSample;
  static const int kNoSpeechTimeoutSec;
  static const int kEndpointerEstimationTimeMs;

 private:
  void InformErrorAndCancelRecognition(SpeechInputError error);
  void SendRecordedAudioToServer();

  void HandleOnError(int error_code);  // Handles OnError in the IO thread.

  // Handles OnData in the IO thread. Takes ownership of |data|.
  void HandleOnData(std::string* data);

  Delegate* delegate_;
  int caller_id_;
  std::string language_;
  std::string grammar_;
  bool filter_profanities_;
  std::string hardware_info_;
  std::string origin_url_;

  scoped_ptr<SpeechRecognitionRequest> request_;
  scoped_refptr<media::AudioInputController> audio_controller_;
  scoped_refptr<net::URLRequestContextGetter> context_getter_;
  AudioEncoder::Codec codec_;
  scoped_ptr<AudioEncoder> encoder_;
  Endpointer endpointer_;
  int num_samples_recorded_;
  float audio_level_;

  DISALLOW_COPY_AND_ASSIGN(SpeechRecognizer);
};

// This typedef is to workaround the issue with certain versions of
// Visual Studio where it gets confused between multiple Delegate
// classes and gives a C2500 error. (I saw this error on the try bots -
// the workaround was not needed for my machine).
typedef SpeechRecognizer::Delegate SpeechRecognizerDelegate;

}  // namespace speech_input

#endif  // CONTENT_BROWSER_SPEECH_SPEECH_RECOGNIZER_H_
