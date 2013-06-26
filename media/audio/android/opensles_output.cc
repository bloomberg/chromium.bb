// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/opensles_output.h"

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "media/audio/android/audio_manager_android.h"

#define LOG_ON_FAILURE_AND_RETURN(op, ...) \
  do { \
    SLresult err = (op);            \
    if (err != SL_RESULT_SUCCESS) { \
      DLOG(ERROR) << #op << " failed: " << err; \
      return __VA_ARGS__; \
    } \
  } while (0)

namespace media {

OpenSLESOutputStream::OpenSLESOutputStream(AudioManagerAndroid* manager,
                                           const AudioParameters& params)
    : audio_manager_(manager),
      callback_(NULL),
      player_(NULL),
      simple_buffer_queue_(NULL),
      active_queue_(0),
      buffer_size_bytes_(0),
      started_(false),
      volume_(1.0) {
  format_.formatType = SL_DATAFORMAT_PCM;
  format_.numChannels = static_cast<SLuint32>(params.channels());
  // Provides sampling rate in milliHertz to OpenSLES.
  format_.samplesPerSec = static_cast<SLuint32>(params.sample_rate() * 1000);
  format_.bitsPerSample = params.bits_per_sample();
  format_.containerSize = params.bits_per_sample();
  format_.endianness = SL_BYTEORDER_LITTLEENDIAN;
  if (format_.numChannels == 1)
    format_.channelMask = SL_SPEAKER_FRONT_CENTER;
  else if (format_.numChannels == 2)
    format_.channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
  else
    NOTREACHED() << "Unsupported number of channels: " << format_.numChannels;

  buffer_size_bytes_ = params.GetBytesPerBuffer();
  audio_bus_ = AudioBus::Create(params);

  memset(&audio_data_, 0, sizeof(audio_data_));
}

OpenSLESOutputStream::~OpenSLESOutputStream() {
  DCHECK(!engine_object_.Get());
  DCHECK(!player_object_.Get());
  DCHECK(!output_mixer_.Get());
  DCHECK(!player_);
  DCHECK(!simple_buffer_queue_);
  DCHECK(!audio_data_[0]);
}

bool OpenSLESOutputStream::Open() {
  if (engine_object_.Get())
    return false;

  if (!CreatePlayer())
    return false;

  SetupAudioBuffer();

  return true;
}

void OpenSLESOutputStream::Start(AudioSourceCallback* callback) {
  DCHECK(callback);
  DCHECK(player_);
  DCHECK(simple_buffer_queue_);
  if (started_)
    return;

  // Enable the flags before streaming.
  callback_ = callback;
  active_queue_ = 0;
  started_ = true;

  // Avoid start-up glitches by filling up one buffer queue before starting
  // the stream.
  FillBufferQueue();

  // Start streaming data by setting the play state to |SL_PLAYSTATE_PLAYING|.
  LOG_ON_FAILURE_AND_RETURN(
      (*player_)->SetPlayState(player_, SL_PLAYSTATE_PLAYING));
}

void OpenSLESOutputStream::Stop() {
  if (!started_)
    return;

  started_ = false;
  // Stop playing by setting the play state to |SL_PLAYSTATE_STOPPED|.
  LOG_ON_FAILURE_AND_RETURN(
      (*player_)->SetPlayState(player_, SL_PLAYSTATE_STOPPED));

  // Clear the buffer queue so that the old data won't be played when
  // resuming playing.
  LOG_ON_FAILURE_AND_RETURN(
      (*simple_buffer_queue_)->Clear(simple_buffer_queue_));
}

void OpenSLESOutputStream::Close() {
  // Stop the stream if it is still playing.
  Stop();

  // Explicitly free the player objects and invalidate their associated
  // interfaces. They have to be done in the correct order.
  player_object_.Reset();
  output_mixer_.Reset();
  engine_object_.Reset();
  simple_buffer_queue_ = NULL;
  player_ = NULL;

  ReleaseAudioBuffer();

  audio_manager_->ReleaseOutputStream(this);
}

void OpenSLESOutputStream::SetVolume(double volume) {
  float volume_float = static_cast<float>(volume);
  if (volume_float < 0.0f || volume_float > 1.0f) {
    return;
  }
  volume_ = volume_float;
}

void OpenSLESOutputStream::GetVolume(double* volume) {
  *volume = static_cast<double>(volume_);
}

bool OpenSLESOutputStream::CreatePlayer() {
  // Initializes the engine object with specific option. After working with the
  // object, we need to free the object and its resources.
  SLEngineOption option[] = {
    { SL_ENGINEOPTION_THREADSAFE, static_cast<SLuint32>(SL_BOOLEAN_TRUE) }
  };
  LOG_ON_FAILURE_AND_RETURN(
      slCreateEngine(engine_object_.Receive(), 1, option, 0, NULL, NULL),
      false);

  // Realize the SL engine object in synchronous mode.
  LOG_ON_FAILURE_AND_RETURN(
      engine_object_->Realize(engine_object_.Get(), SL_BOOLEAN_FALSE),
      false);

  // Get the SL engine interface which is implicit.
  SLEngineItf engine;
  LOG_ON_FAILURE_AND_RETURN(
      engine_object_->GetInterface(engine_object_.Get(),
                                   SL_IID_ENGINE,
                                   &engine),
      false);

  // Create ouput mixer object to be used by the player.
  LOG_ON_FAILURE_AND_RETURN(
      (*engine)->CreateOutputMix(engine,
                                 output_mixer_.Receive(),
                                 0,
                                 NULL,
                                 NULL),
      false);

  // Realizing the output mix object in synchronous mode.
  LOG_ON_FAILURE_AND_RETURN(
      output_mixer_->Realize(output_mixer_.Get(), SL_BOOLEAN_FALSE),
      false);

  // Audio source configuration.
  SLDataLocator_AndroidSimpleBufferQueue simple_buffer_queue = {
    SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
    static_cast<SLuint32>(kNumOfQueuesInBuffer)
  };
  SLDataSource audio_source = { &simple_buffer_queue, &format_ };

  // Audio sink configuration.
  SLDataLocator_OutputMix locator_output_mix = {
    SL_DATALOCATOR_OUTPUTMIX, output_mixer_.Get()
  };
  SLDataSink audio_sink = { &locator_output_mix, NULL };

  // Create an audio player.
  const SLInterfaceID interface_id[] = {
    SL_IID_BUFFERQUEUE,
    SL_IID_VOLUME,
    SL_IID_ANDROIDCONFIGURATION
  };
  const SLboolean interface_required[] = {
    SL_BOOLEAN_TRUE,
    SL_BOOLEAN_TRUE,
    SL_BOOLEAN_TRUE
  };
  LOG_ON_FAILURE_AND_RETURN(
      (*engine)->CreateAudioPlayer(engine,
                                   player_object_.Receive(),
                                   &audio_source,
                                   &audio_sink,
                                   arraysize(interface_id),
                                   interface_id,
                                   interface_required),
      false);

  // Create AudioPlayer and specify SL_IID_ANDROIDCONFIGURATION.
  SLAndroidConfigurationItf player_config;
  LOG_ON_FAILURE_AND_RETURN(
      player_object_->GetInterface(player_object_.Get(),
                                   SL_IID_ANDROIDCONFIGURATION,
                                   &player_config),
      false);

  SLint32 stream_type = SL_ANDROID_STREAM_VOICE;
  LOG_ON_FAILURE_AND_RETURN(
      (*player_config)->SetConfiguration(player_config,
                                         SL_ANDROID_KEY_STREAM_TYPE,
                                         &stream_type, sizeof(SLint32)),
      false);

  // Realize the player object in synchronous mode.
  LOG_ON_FAILURE_AND_RETURN(
      player_object_->Realize(player_object_.Get(), SL_BOOLEAN_FALSE),
      false);

  // Get an implicit player interface.
  LOG_ON_FAILURE_AND_RETURN(
      player_object_->GetInterface(player_object_.Get(), SL_IID_PLAY, &player_),
      false);

  // Get the simple buffer queue interface.
  LOG_ON_FAILURE_AND_RETURN(
      player_object_->GetInterface(player_object_.Get(),
                                   SL_IID_BUFFERQUEUE,
                                   &simple_buffer_queue_),
      false);

  // Register the input callback for the simple buffer queue.
  // This callback will be called when the soundcard needs data.
  LOG_ON_FAILURE_AND_RETURN(
      (*simple_buffer_queue_)->RegisterCallback(simple_buffer_queue_,
                                                SimpleBufferQueueCallback,
                                                this),
      false);

  return true;
}

void OpenSLESOutputStream::SimpleBufferQueueCallback(
    SLAndroidSimpleBufferQueueItf buffer_queue, void* instance) {
  OpenSLESOutputStream* stream =
      reinterpret_cast<OpenSLESOutputStream*>(instance);
  stream->FillBufferQueue();
}

void OpenSLESOutputStream::FillBufferQueue() {
  if (!started_)
    return;

  TRACE_EVENT0("audio", "OpenSLESOutputStream::FillBufferQueue");
  // Read data from the registered client source.
  // TODO(xians): Get an accurate delay estimation.
  uint32 hardware_delay = buffer_size_bytes_;
  int frames_filled = callback_->OnMoreData(
      audio_bus_.get(), AudioBuffersState(0, hardware_delay));
  if (frames_filled <= 0)
    return;  // Audio source is shutting down, or halted on error.
  int num_filled_bytes =
      frames_filled * audio_bus_->channels() * format_.bitsPerSample / 8;
  DCHECK_LE(static_cast<size_t>(num_filled_bytes), buffer_size_bytes_);
  // Note: If this ever changes to output raw float the data must be clipped and
  // sanitized since it may come from an untrusted source such as NaCl.
  audio_bus_->Scale(volume_);
  audio_bus_->ToInterleaved(
      frames_filled, format_.bitsPerSample / 8, audio_data_[active_queue_]);

  // Enqueue the buffer for playback.
  SLresult err = (*simple_buffer_queue_)->Enqueue(
      simple_buffer_queue_,
      audio_data_[active_queue_],
      num_filled_bytes);
  if (SL_RESULT_SUCCESS != err)
    HandleError(err);

  active_queue_ = (active_queue_  + 1) % kNumOfQueuesInBuffer;
}

void OpenSLESOutputStream::SetupAudioBuffer() {
  DCHECK(!audio_data_[0]);
  for (int i = 0; i < kNumOfQueuesInBuffer; ++i) {
    audio_data_[i] = new uint8[buffer_size_bytes_];
  }
}

void OpenSLESOutputStream::ReleaseAudioBuffer() {
  if (audio_data_[0]) {
    for (int i = 0; i < kNumOfQueuesInBuffer; ++i) {
      delete [] audio_data_[i];
      audio_data_[i] = NULL;
    }
  }
}

void OpenSLESOutputStream::HandleError(SLresult error) {
  DLOG(ERROR) << "OpenSLES Output error " << error;
  if (callback_)
    callback_->OnError(this);
}

}  // namespace media
