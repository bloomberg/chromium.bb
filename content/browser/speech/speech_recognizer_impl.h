// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_SPEECH_RECOGNIZER_IMPL_H_
#define CONTENT_BROWSER_SPEECH_SPEECH_RECOGNIZER_IMPL_H_

#include <list>
#include <utility>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/speech/audio_encoder.h"
#include "content/browser/speech/endpointer/endpointer.h"
#include "content/browser/speech/speech_recognition_request.h"
#include "content/public/browser/speech_recognizer.h"
#include "content/public/common/speech_recognition_result.h"
#include "media/audio/audio_input_controller.h"

class AudioManager;

namespace content {
class SpeechRecognitionEventListener;
}

namespace speech {

// Records audio, sends recorded audio to server and translates server response
// to recognition result.
class CONTENT_EXPORT SpeechRecognizerImpl
    : NON_EXPORTED_BASE(public content::SpeechRecognizer),
      public media::AudioInputController::EventHandler,
      public SpeechRecognitionRequestDelegate {
 public:
  static const int kAudioSampleRate;
  static const int kAudioPacketIntervalMs;  // Duration of each audio packet.
  static const ChannelLayout kChannelLayout;
  static const int kNumBitsPerAudioSample;
  static const int kNoSpeechTimeoutSec;
  static const int kEndpointerEstimationTimeMs;

  SpeechRecognizerImpl(content::SpeechRecognitionEventListener* listener,
                       int caller_id,
                       const std::string& language,
                       const std::string& grammar,
                       net::URLRequestContextGetter* context_getter,
                       bool filter_profanities,
                       const std::string& hardware_info,
                       const std::string& origin_url);

  virtual ~SpeechRecognizerImpl();

  // content::SpeechRecognizer methods.
  virtual bool StartRecognition() OVERRIDE;
  virtual void AbortRecognition() OVERRIDE;
  virtual void StopAudioCapture() OVERRIDE;
  virtual bool IsActive() const OVERRIDE;
  virtual bool IsCapturingAudio() const OVERRIDE;

  // AudioInputController::EventHandler methods.
  virtual void OnCreated(media::AudioInputController* controller) OVERRIDE {}
  virtual void OnRecording(media::AudioInputController* controller) OVERRIDE {}
  virtual void OnError(media::AudioInputController* controller,
                       int error_code) OVERRIDE;
  virtual void OnData(media::AudioInputController* controller,
                      const uint8* data,
                      uint32 size) OVERRIDE;

  // SpeechRecognitionRequest::Delegate methods.
  virtual void SetRecognitionResult(
      const content::SpeechRecognitionResult& result) OVERRIDE;

 private:
  friend class SpeechRecognizerImplTest;

  void InformErrorAndAbortRecognition(
      content::SpeechRecognitionErrorCode error);
  void SendRecordedAudioToServer();

  void HandleOnError(int error_code);  // Handles OnError in the IO thread.

  // Handles OnData in the IO thread. Takes ownership of |raw_audio|.
  void HandleOnData(AudioChunk* raw_audio);

  // Helper method which closes the audio controller and blocks until done.
  void CloseAudioControllerSynchronously();

  void SetAudioManagerForTesting(AudioManager* audio_manager);

  content::SpeechRecognitionEventListener* listener_;
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

  DISALLOW_COPY_AND_ASSIGN(SpeechRecognizerImpl);
};

}  // namespace speech

#endif  // CONTENT_BROWSER_SPEECH_SPEECH_RECOGNIZER_IMPL_H_
