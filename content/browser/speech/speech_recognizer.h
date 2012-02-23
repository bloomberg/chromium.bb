// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "content/public/common/speech_input_result.h"
#include "media/audio/audio_input_controller.h"

class AudioManager;

namespace content {
class SpeechRecognizerDelegate;
}

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
  SpeechRecognizer(content::SpeechRecognizerDelegate* delegate,
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
  // completes (i.e. after receiving
  // SpeechRecognizerDelegate::DidCompleteRecognition).
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
  virtual void SetRecognitionResult(
      const content::SpeechInputResult& result) OVERRIDE;

  static const int kAudioSampleRate;
  static const int kAudioPacketIntervalMs;  // Duration of each audio packet.
  static const ChannelLayout kChannelLayout;
  static const int kNumBitsPerAudioSample;
  static const int kNoSpeechTimeoutSec;
  static const int kEndpointerEstimationTimeMs;

 private:
  friend class SpeechRecognizerTest;

  void InformErrorAndCancelRecognition(content::SpeechInputError error);
  void SendRecordedAudioToServer();

  void HandleOnError(int error_code);  // Handles OnError in the IO thread.

  // Handles OnData in the IO thread. Takes ownership of |data|.
  void HandleOnData(std::string* data);

  // Helper method which closes the audio controller and blocks until done.
  void CloseAudioControllerSynchronously();

  void SetAudioManagerForTesting(AudioManager* audio_manager);

  content::SpeechRecognizerDelegate* delegate_;
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
  AudioManager* audio_manager_;

  DISALLOW_COPY_AND_ASSIGN(SpeechRecognizer);
};

}  // namespace speech_input

#endif  // CONTENT_BROWSER_SPEECH_SPEECH_RECOGNIZER_H_
