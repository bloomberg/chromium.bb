// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/speech_recognizer_impl.h"

#include "base/bind.h"
#include "base/time.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/speech/audio_buffer.h"
#include "content/public/browser/speech_recognizer_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/speech_recognition_result.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserMainLoop;
using content::BrowserThread;
using content::SpeechRecognizer;
using content::SpeechRecognizerDelegate;
using media::AudioInputController;
using std::string;

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
    SpeechRecognizerDelegate* delegate,
    int caller_id,
    const std::string& language,
    const std::string& grammar,
    net::URLRequestContextGetter* context_getter,
    bool filter_profanities,
    const std::string& hardware_info,
    const std::string& origin_url) {
  return new speech::SpeechRecognizerImpl(
      delegate, caller_id, language, grammar, context_getter,
      filter_profanities, hardware_info, origin_url);
}

namespace speech {

const int SpeechRecognizerImpl::kAudioSampleRate = 16000;
const int SpeechRecognizerImpl::kAudioPacketIntervalMs = 100;
const ChannelLayout SpeechRecognizerImpl::kChannelLayout = CHANNEL_LAYOUT_MONO;
const int SpeechRecognizerImpl::kNumBitsPerAudioSample = 16;
const int SpeechRecognizerImpl::kNoSpeechTimeoutSec = 8;
const int SpeechRecognizerImpl::kEndpointerEstimationTimeMs = 300;

SpeechRecognizerImpl::SpeechRecognizerImpl(
    SpeechRecognizerDelegate* delegate,
    int caller_id,
    const std::string& language,
    const std::string& grammar,
    net::URLRequestContextGetter* context_getter,
    bool filter_profanities,
    const std::string& hardware_info,
    const std::string& origin_url)
    : delegate_(delegate),
      caller_id_(caller_id),
      language_(language),
      grammar_(grammar),
      filter_profanities_(filter_profanities),
      hardware_info_(hardware_info),
      origin_url_(origin_url),
      context_getter_(context_getter),
      codec_(AudioEncoder::CODEC_FLAC),
      encoder_(NULL),
      endpointer_(kAudioSampleRate),
      num_samples_recorded_(0),
      audio_level_(0.0f),
      audio_manager_(NULL) {
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
  DCHECK(!request_.get() || !request_->HasPendingRequest());
  DCHECK(!encoder_.get());
  endpointer_.EndSession();
}

bool SpeechRecognizerImpl::StartRecording() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!audio_controller_.get());
  DCHECK(!request_.get() || !request_->HasPendingRequest());
  DCHECK(!encoder_.get());

  // The endpointer needs to estimate the environment/background noise before
  // starting to treat the audio as user input. In |HandleOnData| we wait until
  // such time has passed before switching to user input mode.
  endpointer_.SetEnvironmentEstimationMode();

  encoder_.reset(AudioEncoder::Create(codec_, kAudioSampleRate,
                                      kNumBitsPerAudioSample));
  int samples_per_packet = (kAudioSampleRate * kAudioPacketIntervalMs) / 1000;
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR, kChannelLayout,
                         kAudioSampleRate, kNumBitsPerAudioSample,
                         samples_per_packet);
  audio_controller_ = AudioInputController::Create(
      audio_manager_ ? audio_manager_ : BrowserMainLoop::GetAudioManager(),
      this, params);
  DCHECK(audio_controller_.get());
  VLOG(1) << "SpeechRecognizer starting record.";
  num_samples_recorded_ = 0;
  audio_controller_->Record();

  return true;
}

void SpeechRecognizerImpl::CancelRecognition() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(audio_controller_.get() || request_.get());

  // Stop recording if required.
  if (audio_controller_.get()) {
    CloseAudioControllerSynchronously();
  }

  VLOG(1) << "SpeechRecognizer canceling recognition.";
  encoder_.reset();
  request_.reset();
}

void SpeechRecognizerImpl::StopRecording() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // If audio recording has already stopped and we are in recognition phase,
  // silently ignore any more calls to stop recording.
  if (!audio_controller_.get())
    return;

  CloseAudioControllerSynchronously();

  delegate_->DidStopReceivingSpeech(caller_id_);
  delegate_->DidCompleteRecording(caller_id_);

  // UploadAudioChunk requires a non-empty final buffer. So we encode a packet
  // of silence in case encoder had no data already.
  std::vector<short> samples((kAudioSampleRate * kAudioPacketIntervalMs) /
                             1000);
  AudioChunk dummy_chunk(reinterpret_cast<uint8*>(&samples[0]),
                         samples.size() * sizeof(short),
                         encoder_->bits_per_sample() / 8);
  encoder_->Encode(dummy_chunk);
  encoder_->Flush();
  scoped_ptr<AudioChunk> encoded_data(encoder_->GetEncodedDataAndClear());
  DCHECK(!encoded_data->IsEmpty());
  encoder_.reset();

  // If we haven't got any audio yet end the recognition sequence here.
  if (request_ == NULL) {
    // Guard against the delegate freeing us until we finish our job.
    scoped_refptr<SpeechRecognizerImpl> me(this);
    delegate_->DidCompleteRecognition(caller_id_);
  } else {
    request_->UploadAudioChunk(*encoded_data, true /* is_last_chunk */);
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

  InformErrorAndCancelRecognition(content::SPEECH_RECOGNITION_ERROR_AUDIO);
}

void SpeechRecognizerImpl::OnData(AudioInputController* controller,
                                  const uint8* data, uint32 size) {
  if (size == 0)  // This could happen when recording stops and is normal.
    return;
  AudioChunk* raw_audio = new AudioChunk(data, static_cast<size_t>(size),
                                         kNumBitsPerAudioSample / 8);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&SpeechRecognizerImpl::HandleOnData,
                                     this, raw_audio));
}

void SpeechRecognizerImpl::HandleOnData(AudioChunk* raw_audio) {
  scoped_ptr<AudioChunk> free_raw_audio_on_return(raw_audio);
  // Check if we are still recording and if not discard this buffer, as
  // recording might have been stopped after this buffer was posted to the queue
  // by |OnData|.
  if (!audio_controller_.get())
    return;

  bool speech_was_heard_before_packet = endpointer_.DidStartReceivingSpeech();

  encoder_->Encode(*raw_audio);
  float rms;
  endpointer_.ProcessAudio(*raw_audio, &rms);
  bool did_clip = DetectClipping(*raw_audio);
  num_samples_recorded_ += raw_audio->NumSamples();

  if (request_ == NULL) {
    // This was the first audio packet recorded, so start a request to the
    // server to send the data and inform the delegate.
    delegate_->DidStartReceivingAudio(caller_id_);
    request_.reset(new SpeechRecognitionRequest(context_getter_.get(), this));
    request_->Start(language_, grammar_, filter_profanities_,
                    hardware_info_, origin_url_, encoder_->mime_type());
  }

  scoped_ptr<AudioChunk> encoded_data(encoder_->GetEncodedDataAndClear());
  DCHECK(!encoded_data->IsEmpty());
  request_->UploadAudioChunk(*encoded_data, false /* is_last_chunk */);

  if (endpointer_.IsEstimatingEnvironment()) {
    // Check if we have gathered enough audio for the endpointer to do
    // environment estimation and should move on to detect speech/end of speech.
    if (num_samples_recorded_ >= (kEndpointerEstimationTimeMs *
                                  kAudioSampleRate) / 1000) {
      endpointer_.SetUserInputMode();
      delegate_->DidCompleteEnvironmentEstimation(caller_id_);
    }
    return;  // No more processing since we are still estimating environment.
  }

  // Check if we have waited too long without hearing any speech.
  bool speech_was_heard_after_packet = endpointer_.DidStartReceivingSpeech();
  if (!speech_was_heard_after_packet &&
      num_samples_recorded_ >= kNoSpeechTimeoutSec * kAudioSampleRate) {
    InformErrorAndCancelRecognition(
        content::SPEECH_RECOGNITION_ERROR_NO_SPEECH);
    return;
  }

  if (!speech_was_heard_before_packet && speech_was_heard_after_packet)
    delegate_->DidStartReceivingSpeech(caller_id_);

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

  delegate_->SetInputVolume(caller_id_, did_clip ? 1.0f : audio_level_,
                            noise_level);

  if (endpointer_.speech_input_complete())
    StopRecording();
}

void SpeechRecognizerImpl::SetRecognitionResult(
    const content::SpeechRecognitionResult& result) {
  if (result.error != content::SPEECH_RECOGNITION_ERROR_NONE) {
    InformErrorAndCancelRecognition(result.error);
    return;
  }

  // Guard against the delegate freeing us until we finish our job.
  scoped_refptr<SpeechRecognizerImpl> me(this);
  delegate_->SetRecognitionResult(caller_id_, result);
  delegate_->DidCompleteRecognition(caller_id_);
}

void SpeechRecognizerImpl::InformErrorAndCancelRecognition(
    content::SpeechRecognitionErrorCode error) {
  DCHECK_NE(error, content::SPEECH_RECOGNITION_ERROR_NONE);
  CancelRecognition();

  // Guard against the delegate freeing us until we finish our job.
  scoped_refptr<SpeechRecognizerImpl> me(this);
  delegate_->OnRecognizerError(caller_id_, error);
}

void SpeechRecognizerImpl::CloseAudioControllerSynchronously() {
  VLOG(1) << "SpeechRecognizer stopping record.";

  // TODO(satish): investigate the possibility to utilize the closure
  // and switch to async. version of this method. Compare with how
  // it's done in e.g. the AudioRendererHost.
  base::WaitableEvent closed_event(true, false);
  audio_controller_->Close(base::Bind(&base::WaitableEvent::Signal,
                           base::Unretained(&closed_event)));
  closed_event.Wait();
  audio_controller_ = NULL;  // Releases the ref ptr.
}

void SpeechRecognizerImpl::SetAudioManagerForTesting(
    AudioManager* audio_manager) {
  audio_manager_ = audio_manager;
}

}  // namespace speech
