// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/speech_recognizer_impl.h"

#include "base/bind.h"
#include "base/time.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/speech/audio_buffer.h"
#include "content/browser/speech/google_one_shot_remote_engine.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "content/public/browser/speech_recognizer.h"
#include "content/public/common/speech_recognition_error.h"
#include "content/public/common/speech_recognition_result.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserMainLoop;
using content::BrowserThread;
using content::SpeechRecognitionError;
using content::SpeechRecognitionEventListener;
using content::SpeechRecognitionResult;
using content::SpeechRecognizer;
using media::AudioInputController;

namespace {

// The following constants are related to the volume level indicator shown in
// the UI for recorded audio.
// Multiplier used when new volume is greater than previous level.
const float kUpSmoothingFactor = 1.0f;
// Multiplier used when new volume is lesser than previous level.
const float kDownSmoothingFactor = 0.7f;
// RMS dB value of a maximum (unclipped) sine wave for int16 samples.
const float kAudioMeterMaxDb = 90.31f;
// This value corresponds to RMS dB for int16 with 6 most-significant-bits = 0.
// Values lower than this will display as empty level-meter.
const float kAudioMeterMinDb = 30.0f;
const float kAudioMeterDbRange = kAudioMeterMaxDb - kAudioMeterMinDb;

// Maximum level to draw to display unclipped meter. (1.0f displays clipping.)
const float kAudioMeterRangeMaxUnclipped = 47.0f / 48.0f;

// Returns true if more than 5% of the samples are at min or max value.
bool DetectClipping(const speech::AudioChunk& chunk) {
  const int num_samples = chunk.NumSamples();
  const int16* samples = chunk.SamplesData16();
  const int kThreshold = num_samples / 20;
  int clipping_samples = 0;
  for (int i = 0; i < num_samples; ++i) {
    if (samples[i] <= -32767 || samples[i] >= 32767) {
      if (++clipping_samples > kThreshold)
        return true;
    }
  }
  return false;
}

}  // namespace

SpeechRecognizer* SpeechRecognizer::Create(
    SpeechRecognitionEventListener* listener,
    int caller_id,
    const std::string& language,
    const std::string& grammar,
    net::URLRequestContextGetter* context_getter,
    bool filter_profanities,
    const std::string& hardware_info,
    const std::string& origin_url) {
  return new speech::SpeechRecognizerImpl(listener,
                                          caller_id,
                                          language,
                                          grammar,
                                          context_getter,
                                          filter_profanities,
                                          hardware_info,
                                          origin_url);
}

namespace speech {

const int SpeechRecognizerImpl::kAudioSampleRate = 16000;
const ChannelLayout SpeechRecognizerImpl::kChannelLayout = CHANNEL_LAYOUT_MONO;
const int SpeechRecognizerImpl::kNumBitsPerAudioSample = 16;
const int SpeechRecognizerImpl::kNoSpeechTimeoutMs = 8000;
const int SpeechRecognizerImpl::kEndpointerEstimationTimeMs = 300;

SpeechRecognizerImpl::SpeechRecognizerImpl(
    SpeechRecognitionEventListener* listener,
    int caller_id,
    const std::string& language,
    const std::string& grammar,
    net::URLRequestContextGetter* context_getter,
    bool filter_profanities,
    const std::string& hardware_info,
    const std::string& origin_url)
    : listener_(listener),
      testing_audio_manager_(NULL),
      endpointer_(kAudioSampleRate),
      context_getter_(context_getter),
      caller_id_(caller_id),
      language_(language),
      grammar_(grammar),
      filter_profanities_(filter_profanities),
      hardware_info_(hardware_info),
      origin_url_(origin_url),
      num_samples_recorded_(0),
      audio_level_(0.0f) {
  DCHECK(listener_ != NULL);
  endpointer_.set_speech_input_complete_silence_length(
      base::Time::kMicrosecondsPerSecond / 2);
  endpointer_.set_long_speech_input_complete_silence_length(
      base::Time::kMicrosecondsPerSecond);
  endpointer_.set_long_speech_length(3 * base::Time::kMicrosecondsPerSecond);
  endpointer_.StartSession();
}

SpeechRecognizerImpl::~SpeechRecognizerImpl() {
  // Recording should have stopped earlier due to the endpointer or
  // |StopRecording| being called.
  DCHECK(!audio_controller_.get());
  DCHECK(!recognition_engine_.get() ||
         !recognition_engine_->IsRecognitionPending());
  endpointer_.EndSession();
}

void SpeechRecognizerImpl::StartRecognition() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!audio_controller_.get());
  DCHECK(!recognition_engine_.get() ||
         !recognition_engine_->IsRecognitionPending());

  // The endpointer needs to estimate the environment/background noise before
  // starting to treat the audio as user input. In |HandleOnData| we wait until
  // such time has passed before switching to user input mode.
  endpointer_.SetEnvironmentEstimationMode();

  AudioManager* audio_manager = (testing_audio_manager_ != NULL) ?
                                 testing_audio_manager_ :
                                 BrowserMainLoop::GetAudioManager();
  const int samples_per_packet = kAudioSampleRate *
      GoogleOneShotRemoteEngine::kAudioPacketIntervalMs / 1000;
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR, kChannelLayout,
                         kAudioSampleRate, kNumBitsPerAudioSample,
                         samples_per_packet);
  audio_controller_ = AudioInputController::Create(audio_manager, this, params);
  DCHECK(audio_controller_.get());
  VLOG(1) << "SpeechRecognizer starting record.";
  num_samples_recorded_ = 0;
  audio_controller_->Record();
}

void SpeechRecognizerImpl::AbortRecognition() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(audio_controller_.get() || recognition_engine_.get());

  // Stop recording if required.
  if (audio_controller_.get()) {
    CloseAudioControllerAsynchronously();
  }

  VLOG(1) << "SpeechRecognizer canceling recognition.";
  recognition_engine_.reset();
}

void SpeechRecognizerImpl::StopAudioCapture() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // If audio recording has already stopped and we are in recognition phase,
  // silently ignore any more calls to stop recording.
  if (!audio_controller_.get())
    return;

  CloseAudioControllerAsynchronously();
  listener_->OnSoundEnd(caller_id_);
  listener_->OnAudioEnd(caller_id_);

  // If we haven't got any audio yet end the recognition sequence here.
  if (recognition_engine_ == NULL) {
    // Guard against the listener freeing us until we finish our job.
    scoped_refptr<SpeechRecognizerImpl> me(this);
    listener_->OnRecognitionEnd(caller_id_);
  } else {
    recognition_engine_->AudioChunksEnded();
  }
}

// Invoked in the audio thread.
void SpeechRecognizerImpl::OnError(AudioInputController* controller,
                                   int error_code) {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                         base::Bind(&SpeechRecognizerImpl::HandleOnError,
                                    this, error_code));
}

void SpeechRecognizerImpl::HandleOnError(int error_code) {
  LOG(WARNING) << "SpeechRecognizer::HandleOnError, code=" << error_code;

  // Check if we are still recording before canceling recognition, as
  // recording might have been stopped after this error was posted to the queue
  // by |OnError|.
  if (!audio_controller_.get())
    return;

  InformErrorAndAbortRecognition(content::SPEECH_RECOGNITION_ERROR_AUDIO);
}

void SpeechRecognizerImpl::OnData(AudioInputController* controller,
                                  const uint8* data, uint32 size) {
  if (size == 0)  // This could happen when recording stops and is normal.
    return;
  scoped_refptr<AudioChunk> raw_audio(
      new AudioChunk(data,
                     static_cast<size_t>(size),
                     kNumBitsPerAudioSample / 8));
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&SpeechRecognizerImpl::HandleOnData,
                                     this, raw_audio));
}

void SpeechRecognizerImpl::HandleOnData(scoped_refptr<AudioChunk> raw_audio) {
  // Check if we are still recording and if not discard this buffer, as
  // recording might have been stopped after this buffer was posted to the queue
  // by |OnData|.
  if (!audio_controller_.get())
    return;

  bool speech_was_heard_before_packet = endpointer_.DidStartReceivingSpeech();

  float rms;
  endpointer_.ProcessAudio(*raw_audio, &rms);
  bool did_clip = DetectClipping(*raw_audio);
  num_samples_recorded_ += raw_audio->NumSamples();

  if (recognition_engine_ == NULL) {
    // This was the first audio packet recorded, so start a request to the
    // server to send the data and inform the listener.
    listener_->OnAudioStart(caller_id_);
    GoogleOneShotRemoteEngineConfig google_sr_config;
    google_sr_config.language = language_;
    google_sr_config.grammar = grammar_;
    google_sr_config.audio_sample_rate = kAudioSampleRate;
    google_sr_config.audio_num_bits_per_sample = kNumBitsPerAudioSample;
    google_sr_config.filter_profanities = filter_profanities_;
    google_sr_config.hardware_info = hardware_info_;
    google_sr_config.origin_url = origin_url_;
    GoogleOneShotRemoteEngine* google_sr_engine =
        new GoogleOneShotRemoteEngine(context_getter_.get());
    google_sr_engine->SetConfig(google_sr_config);
    recognition_engine_.reset(google_sr_engine);
    recognition_engine_->set_delegate(this);
    recognition_engine_->StartRecognition();
  }

  recognition_engine_->TakeAudioChunk(*raw_audio);

  if (endpointer_.IsEstimatingEnvironment()) {
    // Check if we have gathered enough audio for the endpointer to do
    // environment estimation and should move on to detect speech/end of speech.
    if (num_samples_recorded_ >= (kEndpointerEstimationTimeMs *
                                  kAudioSampleRate) / 1000) {
      endpointer_.SetUserInputMode();
      listener_->OnEnvironmentEstimationComplete(caller_id_);
    }
    return;  // No more processing since we are still estimating environment.
  }

  // Check if we have waited too long without hearing any speech.
  bool speech_was_heard_after_packet = endpointer_.DidStartReceivingSpeech();
  if (!speech_was_heard_after_packet &&
      num_samples_recorded_ >= (kNoSpeechTimeoutMs / 1000) * kAudioSampleRate) {
    InformErrorAndAbortRecognition(
        content::SPEECH_RECOGNITION_ERROR_NO_SPEECH);
    return;
  }

  if (!speech_was_heard_before_packet && speech_was_heard_after_packet)
    listener_->OnSoundStart(caller_id_);

  // Calculate the input volume to display in the UI, smoothing towards the
  // new level.
  float level = (rms - kAudioMeterMinDb) /
      (kAudioMeterDbRange / kAudioMeterRangeMaxUnclipped);
  level = std::min(std::max(0.0f, level), kAudioMeterRangeMaxUnclipped);
  if (level > audio_level_) {
    audio_level_ += (level - audio_level_) * kUpSmoothingFactor;
  } else {
    audio_level_ += (level - audio_level_) * kDownSmoothingFactor;
  }

  float noise_level = (endpointer_.NoiseLevelDb() - kAudioMeterMinDb) /
      (kAudioMeterDbRange / kAudioMeterRangeMaxUnclipped);
  noise_level = std::min(std::max(0.0f, noise_level),
      kAudioMeterRangeMaxUnclipped);

  listener_->OnAudioLevelsChange(caller_id_, did_clip ? 1.0f : audio_level_,
                                 noise_level);

  if (endpointer_.speech_input_complete())
    StopAudioCapture();
}

void SpeechRecognizerImpl::OnAudioClosed(AudioInputController*) {}

void SpeechRecognizerImpl::OnSpeechRecognitionEngineResult(
    const content::SpeechRecognitionResult& result) {
  // Guard against the listener freeing us until we finish our job.
  scoped_refptr<SpeechRecognizerImpl> me(this);
  listener_->OnRecognitionResult(caller_id_, result);
  listener_->OnRecognitionEnd(caller_id_);
}

void SpeechRecognizerImpl::OnSpeechRecognitionEngineError(
    const content::SpeechRecognitionError& error) {
  InformErrorAndAbortRecognition(error.code);
}

void SpeechRecognizerImpl::InformErrorAndAbortRecognition(
    content::SpeechRecognitionErrorCode error) {
  DCHECK_NE(error, content::SPEECH_RECOGNITION_ERROR_NONE);
  AbortRecognition();

  // Guard against the listener freeing us until we finish our job.
  scoped_refptr<SpeechRecognizerImpl> me(this);
  listener_->OnRecognitionError(caller_id_, error);
}

void SpeechRecognizerImpl::CloseAudioControllerAsynchronously() {
  VLOG(1) << "SpeechRecognizer stopping record.";
  // Issues a Close on the audio controller, passing an empty callback. The only
  // purpose of such callback is to keep the audio controller refcounted until
  // Close has completed (in the audio thread) and automatically destroy it
  // afterwards (upon return from OnAudioClosed).
  audio_controller_->Close(base::Bind(&SpeechRecognizerImpl::OnAudioClosed,
                                      this, audio_controller_));
  audio_controller_ = NULL;  // The controller is still refcounted by Bind.
}

bool SpeechRecognizerImpl::IsActive() const {
  return (recognition_engine_.get() != NULL);
}

bool SpeechRecognizerImpl::IsCapturingAudio() const {
  return (audio_controller_.get() != NULL);
}

const SpeechRecognitionEngine&
    SpeechRecognizerImpl::recognition_engine() const {
  return *(recognition_engine_.get());
}

void SpeechRecognizerImpl::SetAudioManagerForTesting(
    AudioManager* audio_manager) {
  testing_audio_manager_ = audio_manager;
}


}  // namespace speech
