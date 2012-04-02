// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_SPEECH_RECOGNIZER_IMPL_H_
#define CONTENT_BROWSER_SPEECH_SPEECH_RECOGNIZER_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/speech/endpointer/endpointer.h"
#include "content/browser/speech/speech_recognition_engine.h"
#include "content/public/browser/speech_recognizer.h"
#include "content/public/common/speech_recognition_error.h"
#include "media/audio/audio_input_controller.h"
#include "net/url_request/url_request_context_getter.h"

namespace content {
class SpeechRecognitionEventListener;
struct SpeechRecognitionResult;
}

namespace media {
class AudioInputController;
}

namespace speech {

// Records audio, sends recorded audio to server and translates server response
// to recognition result.
class CONTENT_EXPORT SpeechRecognizerImpl
    : public NON_EXPORTED_BASE(content::SpeechRecognizer),
      public media::AudioInputController::EventHandler,
      public NON_EXPORTED_BASE(SpeechRecognitionEngineDelegate) {
 public:
  static const int kAudioSampleRate;
  static const ChannelLayout kChannelLayout;
  static const int kNumBitsPerAudioSample;
  static const int kNoSpeechTimeoutMs;
  static const int kEndpointerEstimationTimeMs;

  SpeechRecognizerImpl(
    content::SpeechRecognitionEventListener* listener,
    int caller_id,
    const std::string& language,
    const std::string& grammar,
    net::URLRequestContextGetter* context_getter,
    bool filter_profanities,
    const std::string& hardware_info,
    const std::string& origin_url);
  virtual ~SpeechRecognizerImpl();

  // content::SpeechRecognizer methods.
  virtual void StartRecognition() OVERRIDE;
  virtual void AbortRecognition() OVERRIDE;
  virtual void StopAudioCapture() OVERRIDE;
  virtual bool IsActive() const OVERRIDE;
  virtual bool IsCapturingAudio() const OVERRIDE;
  const SpeechRecognitionEngine& recognition_engine() const;

  // AudioInputController::EventHandler methods.
  virtual void OnCreated(media::AudioInputController* controller) OVERRIDE {}
  virtual void OnRecording(media::AudioInputController* controller) OVERRIDE {}
  virtual void OnError(media::AudioInputController* controller,
                       int error_code) OVERRIDE;
  virtual void OnData(media::AudioInputController* controller,
                      const uint8* data,
                      uint32 size) OVERRIDE;

  // SpeechRecognitionEngineDelegate methods.
  virtual void OnSpeechRecognitionEngineResult(
      const content::SpeechRecognitionResult& result) OVERRIDE;
  virtual void OnSpeechRecognitionEngineError(
      const content::SpeechRecognitionError& error) OVERRIDE;

 private:
  friend class SpeechRecognizerImplTest;

  void InformErrorAndAbortRecognition(
      content::SpeechRecognitionErrorCode error);
  void SendRecordedAudioToServer();

  void HandleOnError(int error_code);  // Handles OnError in the IO thread.

  // Handles OnData in the IO thread.
  void HandleOnData(scoped_refptr<AudioChunk> raw_audio);

  void OnAudioClosed(media::AudioInputController*);

  // Helper method which closes the audio controller and frees it asynchronously
  // without blocking the IO thread.
  void CloseAudioControllerAsynchronously();

  void SetAudioManagerForTesting(AudioManager* audio_manager);

  content::SpeechRecognitionEventListener* listener_;
  AudioManager* testing_audio_manager_;
  scoped_ptr<SpeechRecognitionEngine> recognition_engine_;
  Endpointer endpointer_;
  scoped_refptr<media::AudioInputController> audio_controller_;
  scoped_refptr<net::URLRequestContextGetter> context_getter_;
  int caller_id_;
  std::string language_;
  std::string grammar_;
  bool filter_profanities_;
  std::string hardware_info_;
  std::string origin_url_;
  int num_samples_recorded_;
  float audio_level_;

  DISALLOW_COPY_AND_ASSIGN(SpeechRecognizerImpl);
};

}  // namespace speech

#endif  // CONTENT_BROWSER_SPEECH_SPEECH_RECOGNIZER_IMPL_H_
