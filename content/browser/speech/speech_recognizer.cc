// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/speech_recognizer.h"

#include "base/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "content/browser/browser_thread.h"

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
const float kAudioMeterMinDb = 60.21f;
const float kAudioMeterDbRange = kAudioMeterMaxDb - kAudioMeterMinDb;

// Maximum level to draw to display unclipped meter. (1.0f displays clipping.)
const float kAudioMeterRangeMaxUnclipped = 47.0f / 48.0f;

// Returns true if more than 5% of the samples are at min or max value.
bool Clipping(const int16* samples, int num_samples) {
  int clipping_samples = 0;
  const int kThreshold = num_samples / 20;
  for (int i = 0; i < num_samples; ++i) {
    if (samples[i] <= -32767 || samples[i] >= 32767) {
      if (++clipping_samples > kThreshold)
        return true;
    }
  }
  return false;
}

}  // namespace

namespace speech_input {

const int SpeechRecognizer::kAudioSampleRate = 16000;
const int SpeechRecognizer::kAudioPacketIntervalMs = 100;
const int SpeechRecognizer::kNumAudioChannels = 1;
const int SpeechRecognizer::kNumBitsPerAudioSample = 16;
const int SpeechRecognizer::kNoSpeechTimeoutSec = 8;
const int SpeechRecognizer::kEndpointerEstimationTimeMs = 300;

SpeechRecognizer::SpeechRecognizer(Delegate* delegate,
                                   int caller_id,
                                   const std::string& language,
                                   const std::string& grammar,
                                   const std::string& hardware_info,
                                   const std::string& origin_url)
    : delegate_(delegate),
      caller_id_(caller_id),
      language_(language),
      grammar_(grammar),
      hardware_info_(hardware_info),
      origin_url_(origin_url),
      codec_(AudioEncoder::CODEC_SPEEX),
      encoder_(NULL),
      endpointer_(kAudioSampleRate),
      num_samples_recorded_(0),
      audio_level_(0.0f) {
  endpointer_.set_speech_input_complete_silence_length(
      base::Time::kMicrosecondsPerSecond / 2);
  endpointer_.set_long_speech_input_complete_silence_length(
      base::Time::kMicrosecondsPerSecond);
  endpointer_.set_long_speech_length(3 * base::Time::kMicrosecondsPerSecond);
  endpointer_.StartSession();
}

SpeechRecognizer::~SpeechRecognizer() {
  // Recording should have stopped earlier due to the endpointer or
  // |StopRecording| being called.
  DCHECK(!audio_controller_.get());
  DCHECK(!request_.get() || !request_->HasPendingRequest());
  DCHECK(!encoder_.get());
  endpointer_.EndSession();
}

bool SpeechRecognizer::StartRecording() {
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
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR, kNumAudioChannels,
                         kAudioSampleRate, kNumBitsPerAudioSample,
                         samples_per_packet);
  audio_controller_ = AudioInputController::Create(this, params);
  DCHECK(audio_controller_.get());
  VLOG(1) << "SpeechRecognizer starting record.";
  num_samples_recorded_ = 0;
  audio_controller_->Record();

  return true;
}

void SpeechRecognizer::CancelRecognition() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(audio_controller_.get() || request_.get());

  // Stop recording if required.
  if (audio_controller_.get()) {
    VLOG(1) << "SpeechRecognizer stopping record.";
    audio_controller_->Close();
    audio_controller_ = NULL;  // Releases the ref ptr.
  }

  VLOG(1) << "SpeechRecognizer canceling recognition.";
  encoder_.reset();
  request_.reset();
}

void SpeechRecognizer::StopRecording() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // If audio recording has already stopped and we are in recognition phase,
  // silently ignore any more calls to stop recording.
  if (!audio_controller_.get())
    return;

  VLOG(1) << "SpeechRecognizer stopping record.";
  audio_controller_->Close();
  audio_controller_ = NULL;  // Releases the ref ptr.
  encoder_->Flush();

  delegate_->DidCompleteRecording(caller_id_);

  // Since the http request takes a single string as POST data, allocate
  // one and copy over bytes from the audio buffers to the string.
  // And If we haven't got any audio yet end the recognition sequence here.
  string mime_type = encoder_->mime_type();
  string data;
  encoder_->GetEncodedData(&data);
  encoder_.reset();

  if (data.empty()) {
    // Guard against the delegate freeing us until we finish our job.
    scoped_refptr<SpeechRecognizer> me(this);
    delegate_->DidCompleteRecognition(caller_id_);
  } else {
    DCHECK(!request_.get());
    request_.reset(new SpeechRecognitionRequest(
        Profile::GetDefaultRequestContext(), this));
    request_->Send(language_, grammar_, hardware_info_, origin_url_,
                   mime_type, data);
  }
}

void SpeechRecognizer::ReleaseAudioBuffers() {
}

// Invoked in the audio thread.
void SpeechRecognizer::OnError(AudioInputController* controller,
                               int error_code) {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                         NewRunnableMethod(this,
                                           &SpeechRecognizer::HandleOnError,
                                           error_code));
}

void SpeechRecognizer::HandleOnError(int error_code) {
  LOG(WARNING) << "SpeechRecognizer::HandleOnError, code=" << error_code;

  // Check if we are still recording before canceling recognition, as
  // recording might have been stopped after this error was posted to the queue
  // by |OnError|.
  if (!audio_controller_.get())
    return;

  InformErrorAndCancelRecognition(RECOGNIZER_ERROR_CAPTURE);
}

void SpeechRecognizer::OnData(AudioInputController* controller,
                              const uint8* data, uint32 size) {
  if (size == 0)  // This could happen when recording stops and is normal.
    return;

  string* str_data = new string(reinterpret_cast<const char*>(data), size);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                         NewRunnableMethod(this,
                                           &SpeechRecognizer::HandleOnData,
                                           str_data));
}

void SpeechRecognizer::HandleOnData(string* data) {
  // Check if we are still recording and if not discard this buffer, as
  // recording might have been stopped after this buffer was posted to the queue
  // by |OnData|.
  if (!audio_controller_.get()) {
    delete data;
    return;
  }

  const short* samples = reinterpret_cast<const short*>(data->data());
  DCHECK((data->length() % sizeof(short)) == 0);
  int num_samples = data->length() / sizeof(short);

  encoder_->Encode(samples, num_samples);
  float rms;
  endpointer_.ProcessAudio(samples, num_samples, &rms);
  bool did_clip = Clipping(samples, num_samples);
  delete data;
  num_samples_recorded_ += num_samples;

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
  if (!endpointer_.DidStartReceivingSpeech() &&
      num_samples_recorded_ >= kNoSpeechTimeoutSec * kAudioSampleRate) {
    InformErrorAndCancelRecognition(RECOGNIZER_ERROR_NO_SPEECH);
    return;
  }

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

  if (endpointer_.speech_input_complete()) {
    StopRecording();
  }

  // TODO(satish): Once we have streaming POST, start sending the data received
  // here as POST chunks.
}

void SpeechRecognizer::SetRecognitionResult(
    bool error, const SpeechInputResultArray& result) {
  if (result.empty()) {
    InformErrorAndCancelRecognition(RECOGNIZER_ERROR_NO_RESULTS);
    return;
  }

  delegate_->SetRecognitionResult(caller_id_, error, result);

  // Guard against the delegate freeing us until we finish our job.
  scoped_refptr<SpeechRecognizer> me(this);
  delegate_->DidCompleteRecognition(caller_id_);
}

void SpeechRecognizer::InformErrorAndCancelRecognition(ErrorCode error) {
  CancelRecognition();

  // Guard against the delegate freeing us until we finish our job.
  scoped_refptr<SpeechRecognizer> me(this);
  delegate_->OnRecognizerError(caller_id_, error);
}

}  // namespace speech_input
