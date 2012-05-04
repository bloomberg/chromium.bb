// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/speech_recognizer_impl.h"

#include "base/basictypes.h"
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
using media::AudioManager;
using media::AudioParameters;

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

void KeepAudioControllerRefcountedForDtor(scoped_refptr<AudioInputController>) {
}

}  // namespace

// TODO(primiano) Create(...) is transitional (until we fix speech input
// extensions) and should be removed soon. The manager should be the only one
// knowing the existence of SpeechRecognizer(Impl), thus the only one in charge
// of instantiating it.
SpeechRecognizer* SpeechRecognizer::Create(
    SpeechRecognitionEventListener* listener,
    int session_id,
    const std::string& language,
    const std::string& grammar,
    net::URLRequestContextGetter* context_getter,
    bool filter_profanities,
    const std::string& hardware_info,
    const std::string& origin_url) {
  speech::GoogleOneShotRemoteEngineConfig remote_engine_config;
  remote_engine_config.language = language;
  remote_engine_config.grammar = grammar;
  remote_engine_config.audio_sample_rate =
      speech::SpeechRecognizerImpl::kAudioSampleRate;
  remote_engine_config.audio_num_bits_per_sample =
      speech::SpeechRecognizerImpl::kNumBitsPerAudioSample;
  remote_engine_config.filter_profanities = filter_profanities;
  remote_engine_config.hardware_info = hardware_info;
  remote_engine_config.origin_url = origin_url;

  // SpeechRecognizerImpl takes ownership of google_remote_engine.
  speech::GoogleOneShotRemoteEngine* google_remote_engine =
      new speech::GoogleOneShotRemoteEngine(context_getter);
  google_remote_engine->SetConfig(remote_engine_config);

  return new speech::SpeechRecognizerImpl(listener,
                                          session_id,
                                          google_remote_engine);
}

namespace speech {

const int SpeechRecognizerImpl::kAudioSampleRate = 16000;
const ChannelLayout SpeechRecognizerImpl::kChannelLayout = CHANNEL_LAYOUT_MONO;
const int SpeechRecognizerImpl::kNumBitsPerAudioSample = 16;
const int SpeechRecognizerImpl::kNoSpeechTimeoutMs = 8000;
const int SpeechRecognizerImpl::kEndpointerEstimationTimeMs = 300;

COMPILE_ASSERT(SpeechRecognizerImpl::kNumBitsPerAudioSample % 8 == 0,
               kNumBitsPerAudioSample_must_be_a_multiple_of_8);

SpeechRecognizerImpl::SpeechRecognizerImpl(
    SpeechRecognitionEventListener* listener,
    int session_id,
    SpeechRecognitionEngine* engine)
    : listener_(listener),
      testing_audio_manager_(NULL),
      recognition_engine_(engine),
      endpointer_(kAudioSampleRate),
      session_id_(session_id),
      is_dispatching_event_(false),
      state_(STATE_IDLE) {
  DCHECK(listener_ != NULL);
  DCHECK(recognition_engine_ != NULL);
  endpointer_.set_speech_input_complete_silence_length(
      base::Time::kMicrosecondsPerSecond / 2);
  endpointer_.set_long_speech_input_complete_silence_length(
      base::Time::kMicrosecondsPerSecond);
  endpointer_.set_long_speech_length(3 * base::Time::kMicrosecondsPerSecond);
  endpointer_.StartSession();
  recognition_engine_->set_delegate(this);
}

// -------  Methods that trigger Finite State Machine (FSM) events ------------

// NOTE:all the external events and requests should be enqueued (PostTask), even
// if they come from the same (IO) thread, in order to preserve the relationship
// of causality between events and avoid interleaved event processing due to
// synchronous callbacks.

void SpeechRecognizerImpl::StartRecognition() {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&SpeechRecognizerImpl::DispatchEvent,
                                     this, FSMEventArgs(EVENT_START)));
}

void SpeechRecognizerImpl::AbortRecognition() {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&SpeechRecognizerImpl::DispatchEvent,
                                     this, FSMEventArgs(EVENT_ABORT)));
}

void SpeechRecognizerImpl::StopAudioCapture() {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&SpeechRecognizerImpl::DispatchEvent,
                                     this, FSMEventArgs(EVENT_STOP_CAPTURE)));
}

bool SpeechRecognizerImpl::IsActive() const {
  // Checking the FSM state from another thread (thus, while the FSM is
  // potentially concurrently evolving) is meaningless.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return state_ != STATE_IDLE;
}

bool SpeechRecognizerImpl::IsCapturingAudio() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO)); // See IsActive().
  const bool is_capturing_audio = state_ >= STATE_STARTING &&
                                  state_ <= STATE_RECOGNIZING;
  DCHECK((is_capturing_audio && (audio_controller_.get() != NULL)) ||
         (!is_capturing_audio && audio_controller_.get() == NULL));
  return is_capturing_audio;
}

const SpeechRecognitionEngine&
SpeechRecognizerImpl::recognition_engine() const {
  return *(recognition_engine_.get());
}

SpeechRecognizerImpl::~SpeechRecognizerImpl() {
  endpointer_.EndSession();
  if (audio_controller_.get()) {
    audio_controller_->Close(base::Bind(&KeepAudioControllerRefcountedForDtor,
                                        audio_controller_));
  }
}

// Invoked in the audio thread.
void SpeechRecognizerImpl::OnError(AudioInputController* controller,
                                   int error_code) {
  FSMEventArgs event_args(EVENT_AUDIO_ERROR);
  event_args.audio_error_code = error_code;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&SpeechRecognizerImpl::DispatchEvent,
                                     this, event_args));
}

void SpeechRecognizerImpl::OnData(AudioInputController* controller,
                                  const uint8* data, uint32 size) {
  if (size == 0)  // This could happen when audio capture stops and is normal.
    return;

  FSMEventArgs event_args(EVENT_AUDIO_DATA);
  event_args.audio_data = new AudioChunk(data, static_cast<size_t>(size),
                                         kNumBitsPerAudioSample / 8);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&SpeechRecognizerImpl::DispatchEvent,
                                     this, event_args));
}

void SpeechRecognizerImpl::OnAudioClosed(AudioInputController*) {}

void SpeechRecognizerImpl::OnSpeechRecognitionEngineResult(
    const content::SpeechRecognitionResult& result) {
  FSMEventArgs event_args(EVENT_ENGINE_RESULT);
  event_args.engine_result = result;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&SpeechRecognizerImpl::DispatchEvent,
                                     this, event_args));
}

void SpeechRecognizerImpl::OnSpeechRecognitionEngineError(
    const content::SpeechRecognitionError& error) {
  FSMEventArgs event_args(EVENT_ENGINE_ERROR);
  event_args.engine_error = error;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&SpeechRecognizerImpl::DispatchEvent,
                                     this, event_args));
}

// -----------------------  Core FSM implementation ---------------------------
// TODO(primiano) After the changes in the media package (r129173), this class
// slightly violates the SpeechRecognitionEventListener interface contract. In
// particular, it is not true anymore that this class can be freed after the
// OnRecognitionEnd event, since the audio_controller_.Close() asynchronous
// call can be still in progress after the end event. Currently, it does not
// represent a problem for the browser itself, since refcounting protects us
// against such race conditions. However, we should fix this in the next CLs.
// For instance, tests are currently working just because the
// TestAudioInputController is not closing asynchronously as the real controller
// does, but they will become flaky if TestAudioInputController will be fixed.

void SpeechRecognizerImpl::DispatchEvent(const FSMEventArgs& event_args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_LE(event_args.event, EVENT_MAX_VALUE);
  DCHECK_LE(state_, STATE_MAX_VALUE);

  // Event dispatching must be sequential, otherwise it will break all the rules
  // and the assumptions of the finite state automata model.
  DCHECK(!is_dispatching_event_);
  is_dispatching_event_ = true;

  // Guard against the delegate freeing us until we finish processing the event.
  scoped_refptr<SpeechRecognizerImpl> me(this);

  if (event_args.event == EVENT_AUDIO_DATA) {
    DCHECK(event_args.audio_data.get() != NULL);
    ProcessAudioPipeline(*event_args.audio_data);
  }

  // The audio pipeline must be processed before the event dispatch, otherwise
  // it would take actions according to the future state instead of the current.
  state_ = ExecuteTransitionAndGetNextState(event_args);

  is_dispatching_event_ = false;
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::ExecuteTransitionAndGetNextState(
    const FSMEventArgs& event_args) {
  const FSMEvent event = event_args.event;
  switch (state_) {
    case STATE_IDLE:
      switch (event) {
        // TODO(primiano) restore UNREACHABLE_CONDITION on EVENT_ABORT and
        // EVENT_STOP_CAPTURE below once speech input extensions are fixed.
        case EVENT_ABORT:
          return DoNothing(event_args);
        case EVENT_START:
          return StartRecording(event_args);
        case EVENT_STOP_CAPTURE:  // Corner cases related to queued messages
        case EVENT_AUDIO_DATA:    // being lately dispatched.
        case EVENT_ENGINE_RESULT:
        case EVENT_ENGINE_ERROR:
        case EVENT_AUDIO_ERROR:
          return DoNothing(event_args);
      }
      break;
    case STATE_STARTING:
      switch (event) {
        case EVENT_ABORT:
          return Abort(event_args);
        case EVENT_START:
          return NotFeasible(event_args);
        case EVENT_STOP_CAPTURE:
          return Abort(event_args);
        case EVENT_AUDIO_DATA:
          return StartRecognitionEngine(event_args);
        case EVENT_ENGINE_RESULT:
          return NotFeasible(event_args);
        case EVENT_ENGINE_ERROR:
        case EVENT_AUDIO_ERROR:
          return Abort(event_args);
      }
      break;
    case STATE_ESTIMATING_ENVIRONMENT:
      switch (event) {
        case EVENT_ABORT:
          return Abort(event_args);
        case EVENT_START:
          return NotFeasible(event_args);
        case EVENT_STOP_CAPTURE:
          return StopCaptureAndWaitForResult(event_args);
        case EVENT_AUDIO_DATA:
          return WaitEnvironmentEstimationCompletion(event_args);
        case EVENT_ENGINE_RESULT:
          return ProcessIntermediateResult(event_args);
        case EVENT_ENGINE_ERROR:
        case EVENT_AUDIO_ERROR:
          return Abort(event_args);
      }
      break;
    case STATE_WAITING_FOR_SPEECH:
      switch (event) {
        case EVENT_ABORT:
          return Abort(event_args);
        case EVENT_START:
          return NotFeasible(event_args);
        case EVENT_STOP_CAPTURE:
          return StopCaptureAndWaitForResult(event_args);
        case EVENT_AUDIO_DATA:
          return DetectUserSpeechOrTimeout(event_args);
        case EVENT_ENGINE_RESULT:
          return ProcessIntermediateResult(event_args);
        case EVENT_ENGINE_ERROR:
        case EVENT_AUDIO_ERROR:
          return Abort(event_args);
      }
      break;
    case STATE_RECOGNIZING:
      switch (event) {
        case EVENT_ABORT:
          return Abort(event_args);
        case EVENT_START:
          return NotFeasible(event_args);
        case EVENT_STOP_CAPTURE:
          return StopCaptureAndWaitForResult(event_args);
        case EVENT_AUDIO_DATA:
          return DetectEndOfSpeech(event_args);
        case EVENT_ENGINE_RESULT:
          return ProcessIntermediateResult(event_args);
        case EVENT_ENGINE_ERROR:
        case EVENT_AUDIO_ERROR:
          return Abort(event_args);
      }
      break;
    case STATE_WAITING_FINAL_RESULT:
      switch (event) {
        case EVENT_ABORT:
          return Abort(event_args);
        case EVENT_START:
          return NotFeasible(event_args);
        case EVENT_STOP_CAPTURE:
        case EVENT_AUDIO_DATA:
          return DoNothing(event_args);
        case EVENT_ENGINE_RESULT:
          return ProcessFinalResult(event_args);
        case EVENT_ENGINE_ERROR:
        case EVENT_AUDIO_ERROR:
          return Abort(event_args);
      }
      break;
  }
  return NotFeasible(event_args);
}

// ----------- Contract for all the FSM evolution functions below -------------
//  - Are guaranteed to be executed in the IO thread;
//  - Are guaranteed to be not reentrant (themselves and each other);
//  - event_args members are guaranteed to be stable during the call;
//  - The class won't be freed in the meanwhile due to callbacks;
//  - IsCapturingAudio() returns true if and only if audio_controller_ != NULL.

// TODO(primiano) the audio pipeline is currently serial. However, the
// clipper->endpointer->vumeter chain and the sr_engine could be parallelized.
// We should profile the execution to see if it would be worth or not.
void SpeechRecognizerImpl::ProcessAudioPipeline(const AudioChunk& raw_audio) {
  const bool route_to_endpointer = state_ >= STATE_ESTIMATING_ENVIRONMENT &&
                                   state_ <= STATE_RECOGNIZING;
  const bool route_to_sr_engine = route_to_endpointer;
  const bool route_to_vumeter = state_ >= STATE_WAITING_FOR_SPEECH &&
                                state_ <= STATE_RECOGNIZING;
  const bool clip_detected = DetectClipping(raw_audio);
  float rms = 0.0f;

  num_samples_recorded_ += raw_audio.NumSamples();

  if (route_to_endpointer)
    endpointer_.ProcessAudio(raw_audio, &rms);

  if (route_to_vumeter) {
    DCHECK(route_to_endpointer); // Depends on endpointer due to |rms|.
    UpdateSignalAndNoiseLevels(rms, clip_detected);
  }
  if (route_to_sr_engine) {
    DCHECK(recognition_engine_.get() != NULL);
    recognition_engine_->TakeAudioChunk(raw_audio);
  }
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::StartRecording(const FSMEventArgs&) {
  DCHECK(recognition_engine_.get() != NULL);
  DCHECK(!IsCapturingAudio());
  AudioManager* audio_manager = (testing_audio_manager_ != NULL) ?
                                 testing_audio_manager_ :
                                 BrowserMainLoop::GetAudioManager();
  DCHECK(audio_manager != NULL);

  DVLOG(1) << "SpeechRecognizerImpl starting audio capture.";
  num_samples_recorded_ = 0;
  audio_level_ = 0;
  listener_->OnRecognitionStart(session_id_);

  if (!audio_manager->HasAudioInputDevices()) {
    return AbortWithError(SpeechRecognitionError(
        content::SPEECH_RECOGNITION_ERROR_AUDIO,
        content::SPEECH_AUDIO_ERROR_DETAILS_NO_MIC));
  }

  if (audio_manager->IsRecordingInProcess()) {
    return AbortWithError(SpeechRecognitionError(
        content::SPEECH_RECOGNITION_ERROR_AUDIO,
        content::SPEECH_AUDIO_ERROR_DETAILS_IN_USE));
  }

  const int samples_per_packet = (kAudioSampleRate *
      recognition_engine_->GetDesiredAudioChunkDurationMs()) / 1000;
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR, kChannelLayout,
                         kAudioSampleRate, kNumBitsPerAudioSample,
                         samples_per_packet);
  audio_controller_ = AudioInputController::Create(audio_manager, this, params);

  if (audio_controller_.get() == NULL) {
    return AbortWithError(
        SpeechRecognitionError(content::SPEECH_RECOGNITION_ERROR_AUDIO));
  }

  // The endpointer needs to estimate the environment/background noise before
  // starting to treat the audio as user input. We wait in the state
  // ESTIMATING_ENVIRONMENT until such interval has elapsed before switching
  // to user input mode.
  endpointer_.SetEnvironmentEstimationMode();
  audio_controller_->Record();
  return STATE_STARTING;
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::StartRecognitionEngine(const FSMEventArgs& event_args) {
  // This is the first audio packet captured, so the recognition engine is
  // started and the delegate notified about the event.
  DCHECK(recognition_engine_.get() != NULL);
  recognition_engine_->StartRecognition();
  listener_->OnAudioStart(session_id_);

  // This is a little hack, since TakeAudioChunk() is already called by
  // ProcessAudioPipeline(). It is the best tradeoff, unless we allow dropping
  // the first audio chunk captured after opening the audio device.
  recognition_engine_->TakeAudioChunk(*(event_args.audio_data));
  return STATE_ESTIMATING_ENVIRONMENT;
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::WaitEnvironmentEstimationCompletion(const FSMEventArgs&) {
  DCHECK(endpointer_.IsEstimatingEnvironment());
  if (GetElapsedTimeMs() >= kEndpointerEstimationTimeMs) {
    endpointer_.SetUserInputMode();
    listener_->OnEnvironmentEstimationComplete(session_id_);
    return STATE_WAITING_FOR_SPEECH;
  } else {
    return STATE_ESTIMATING_ENVIRONMENT;
  }
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::DetectUserSpeechOrTimeout(const FSMEventArgs&) {
  if (endpointer_.DidStartReceivingSpeech()) {
    listener_->OnSoundStart(session_id_);
    return STATE_RECOGNIZING;
  } else if (GetElapsedTimeMs() >= kNoSpeechTimeoutMs) {
    return AbortWithError(
        SpeechRecognitionError(content::SPEECH_RECOGNITION_ERROR_NO_SPEECH));
  }
  return STATE_WAITING_FOR_SPEECH;
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::DetectEndOfSpeech(const FSMEventArgs& event_args) {
  if (endpointer_.speech_input_complete()) {
    return StopCaptureAndWaitForResult(event_args);
  }
  return STATE_RECOGNIZING;
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::StopCaptureAndWaitForResult(const FSMEventArgs&) {
  DCHECK(state_ >= STATE_ESTIMATING_ENVIRONMENT && state_ <= STATE_RECOGNIZING);

  DVLOG(1) << "Concluding recognition";
  CloseAudioControllerAsynchronously();
  recognition_engine_->AudioChunksEnded();

  if (state_ > STATE_WAITING_FOR_SPEECH)
    listener_->OnSoundEnd(session_id_);

  listener_->OnAudioEnd(session_id_);
  return STATE_WAITING_FINAL_RESULT;
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::Abort(const FSMEventArgs& event_args) {
  // TODO(primiano) Should raise SPEECH_RECOGNITION_ERROR_ABORTED in lack of
  // other specific error sources (so that it was an explicit abort request).
  // However, SPEECH_RECOGNITION_ERROR_ABORTED is not currently caught by
  // ChromeSpeechRecognitionManagerDelegate and would cause an exception.
  // JS support will probably need it in future.
  if (event_args.event == EVENT_AUDIO_ERROR) {
    return AbortWithError(
        SpeechRecognitionError(content::SPEECH_RECOGNITION_ERROR_AUDIO));
  } else if (event_args.event == EVENT_ENGINE_ERROR) {
    return AbortWithError(event_args.engine_error);
  }
  return AbortWithError(NULL);
}

SpeechRecognizerImpl::FSMState SpeechRecognizerImpl::AbortWithError(
    const SpeechRecognitionError& error) {
  return AbortWithError(&error);
}

SpeechRecognizerImpl::FSMState SpeechRecognizerImpl::AbortWithError(
    const SpeechRecognitionError* error) {
  if (IsCapturingAudio())
    CloseAudioControllerAsynchronously();

  DVLOG(1) << "SpeechRecognizerImpl canceling recognition. ";

  // The recognition engine is initialized only after STATE_STARTING.
  if (state_ > STATE_STARTING) {
    DCHECK(recognition_engine_.get() != NULL);
    recognition_engine_->EndRecognition();
  }

  if (state_ > STATE_WAITING_FOR_SPEECH && state_ < STATE_WAITING_FINAL_RESULT)
    listener_->OnSoundEnd(session_id_);

  if (state_ > STATE_STARTING && state_ < STATE_WAITING_FINAL_RESULT)
    listener_->OnAudioEnd(session_id_);

  if (error != NULL)
    listener_->OnRecognitionError(session_id_, *error);

  listener_->OnRecognitionEnd(session_id_);

  return STATE_IDLE;
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::ProcessIntermediateResult(const FSMEventArgs&) {
  // This is in preparation for future speech recognition functions.
  NOTREACHED();
  return state_;
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::ProcessFinalResult(const FSMEventArgs& event_args) {
  const SpeechRecognitionResult& result = event_args.engine_result;
  DVLOG(1) << "Got valid result";
  recognition_engine_->EndRecognition();
  listener_->OnRecognitionResult(session_id_, result);
  listener_->OnRecognitionEnd(session_id_);
  return STATE_IDLE;
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::DoNothing(const FSMEventArgs&) const {
  return state_;  // Just keep the current state.
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::NotFeasible(const FSMEventArgs& event_args) {
  NOTREACHED() << "Unfeasible event " << event_args.event
               << " in state " << state_;
  return state_;
}

void SpeechRecognizerImpl::CloseAudioControllerAsynchronously() {
  DCHECK(IsCapturingAudio());
  DVLOG(1) << "SpeechRecognizerImpl stopping audio capture.";
  // Issues a Close on the audio controller, passing an empty callback. The only
  // purpose of such callback is to keep the audio controller refcounted until
  // Close has completed (in the audio thread) and automatically destroy it
  // afterwards (upon return from OnAudioClosed).
  audio_controller_->Close(base::Bind(&SpeechRecognizerImpl::OnAudioClosed,
                                      this, audio_controller_));
  audio_controller_ = NULL;  // The controller is still refcounted by Bind.
}

int SpeechRecognizerImpl::GetElapsedTimeMs() const {
  return (num_samples_recorded_ * 1000) / kAudioSampleRate;
}

void SpeechRecognizerImpl::UpdateSignalAndNoiseLevels(const float& rms,
                                                      bool clip_detected) {
  // Calculate the input volume to display in the UI, smoothing towards the
  // new level.
  // TODO(primiano) Do we really need all this floating point arith here?
  // Perhaps it might be quite expensive on mobile.
  float level = (rms - kAudioMeterMinDb) /
      (kAudioMeterDbRange / kAudioMeterRangeMaxUnclipped);
  level = std::min(std::max(0.0f, level), kAudioMeterRangeMaxUnclipped);
  const float smoothing_factor = (level > audio_level_) ? kUpSmoothingFactor :
                                                          kDownSmoothingFactor;
  audio_level_ += (level - audio_level_) * smoothing_factor;

  float noise_level = (endpointer_.NoiseLevelDb() - kAudioMeterMinDb) /
      (kAudioMeterDbRange / kAudioMeterRangeMaxUnclipped);
  noise_level = std::min(std::max(0.0f, noise_level),
                         kAudioMeterRangeMaxUnclipped);

  listener_->OnAudioLevelsChange(
      session_id_, clip_detected ? 1.0f : audio_level_, noise_level);
}

void SpeechRecognizerImpl::SetAudioManagerForTesting(
    AudioManager* audio_manager) {
  testing_audio_manager_ = audio_manager;
}

SpeechRecognizerImpl::FSMEventArgs::FSMEventArgs(FSMEvent event_value)
    : event(event_value),
      audio_error_code(0),
      audio_data(NULL),
      engine_error(content::SPEECH_RECOGNITION_ERROR_NONE) {
}

SpeechRecognizerImpl::FSMEventArgs::~FSMEventArgs() {
}

}  // namespace speech
