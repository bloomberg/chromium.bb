// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_controller.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "build/build_config.h"
#include "media/audio/shared_memory_util.h"

using base::Time;
using base::TimeDelta;

namespace media {

// Polling-related constants.
const int AudioOutputController::kPollNumAttempts = 3;
const int AudioOutputController::kPollPauseInMilliseconds = 3;

AudioOutputController::AudioOutputController(AudioManager* audio_manager,
                                             EventHandler* handler,
                                             const AudioParameters& params,
                                             SyncReader* sync_reader)
    : audio_manager_(audio_manager),
      params_(params),
      handler_(handler),
      stream_(NULL),
      diverting_to_stream_(NULL),
      volume_(1.0),
      state_(kEmpty),
      num_allowed_io_(0),
      sync_reader_(sync_reader),
      message_loop_(audio_manager->GetMessageLoop()),
      number_polling_attempts_left_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_this_(this)) {
  DCHECK(audio_manager);
  DCHECK(handler_);
  DCHECK(sync_reader_);
  DCHECK(message_loop_);
}

AudioOutputController::~AudioOutputController() {
  DCHECK_EQ(kClosed, state_);
}

// static
scoped_refptr<AudioOutputController> AudioOutputController::Create(
    AudioManager* audio_manager,
    EventHandler* event_handler,
    const AudioParameters& params,
    SyncReader* sync_reader) {
  DCHECK(audio_manager);
  DCHECK(sync_reader);

  if (!params.IsValid() || !audio_manager)
    return NULL;

  scoped_refptr<AudioOutputController> controller(new AudioOutputController(
      audio_manager, event_handler, params, sync_reader));
  controller->message_loop_->PostTask(FROM_HERE, base::Bind(
      &AudioOutputController::DoCreate, controller, false));
  return controller;
}

void AudioOutputController::Play() {
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &AudioOutputController::DoPlay, this));
}

void AudioOutputController::Pause() {
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &AudioOutputController::DoPause, this));
}

void AudioOutputController::Flush() {
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &AudioOutputController::DoFlush, this));
}

void AudioOutputController::Close(const base::Closure& closed_task) {
  DCHECK(!closed_task.is_null());
  message_loop_->PostTaskAndReply(FROM_HERE, base::Bind(
      &AudioOutputController::DoClose, this), closed_task);
}

void AudioOutputController::SetVolume(double volume) {
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &AudioOutputController::DoSetVolume, this, volume));
}

void AudioOutputController::DoCreate(bool is_for_device_change) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // Close() can be called before DoCreate() is executed.
  if (state_ == kClosed)
    return;

  DoStopCloseAndClearStream();  // Calls RemoveOutputDeviceChangeListener().
  DCHECK_EQ(kEmpty, state_);

  stream_ = diverting_to_stream_ ? diverting_to_stream_ :
      audio_manager_->MakeAudioOutputStreamProxy(params_);
  if (!stream_) {
    state_ = kError;

    // TODO(hclam): Define error types.
    handler_->OnError(this, 0);
    return;
  }

  if (!stream_->Open()) {
    DoStopCloseAndClearStream();
    state_ = kError;

    // TODO(hclam): Define error types.
    handler_->OnError(this, 0);
    return;
  }

  // Everything started okay, so re-register for state change callbacks if
  // stream_ was created via AudioManager.
  if (stream_ != diverting_to_stream_)
    audio_manager_->AddOutputDeviceChangeListener(this);

  // We have successfully opened the stream. Set the initial volume.
  stream_->SetVolume(volume_);

  // Finally set the state to kCreated.
  state_ = kCreated;

  // And then report we have been created if we haven't done so already.
  if (!is_for_device_change)
    handler_->OnCreated(this);
}

void AudioOutputController::DoPlay() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // We can start from created or paused state.
  if (state_ != kCreated && state_ != kPaused)
    return;

  state_ = kStarting;

  // Ask for first packet.
  sync_reader_->UpdatePendingBytes(0);

  // Cannot start stream immediately, should give renderer some time
  // to deliver data.
  // TODO(vrk): The polling here and in WaitTillDataReady() is pretty clunky.
  // Refine the API such that polling is no longer needed. (crbug.com/112196)
  number_polling_attempts_left_ = kPollNumAttempts;
  DCHECK(!weak_this_.HasWeakPtrs());
  message_loop_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&AudioOutputController::PollAndStartIfDataReady,
      weak_this_.GetWeakPtr()),
      TimeDelta::FromMilliseconds(kPollPauseInMilliseconds));
}

void AudioOutputController::PollAndStartIfDataReady() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  DCHECK_EQ(kStarting, state_);

  // If we are ready to start the stream, start it.
  if (--number_polling_attempts_left_ == 0 ||
      sync_reader_->DataReady()) {
    StartStream();
  } else {
    message_loop_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&AudioOutputController::PollAndStartIfDataReady,
        weak_this_.GetWeakPtr()),
        TimeDelta::FromMilliseconds(kPollPauseInMilliseconds));
  }
}

void AudioOutputController::StartStream() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  state_ = kPlaying;

  // We start the AudioOutputStream lazily.
  AllowEntryToOnMoreIOData();
  stream_->Start(this);

  // Tell the event handler that we are now playing.
  handler_->OnPlaying(this);
}

void AudioOutputController::StopStream() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (state_ == kStarting) {
    // Cancel in-progress polling start.
    weak_this_.InvalidateWeakPtrs();
    state_ = kPaused;
  } else if (state_ == kPlaying) {
    stream_->Stop();
    DisallowEntryToOnMoreIOData();
    state_ = kPaused;
  }
}

void AudioOutputController::DoPause() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  StopStream();

  if (state_ != kPaused)
    return;

  // Send a special pause mark to the low-latency audio thread.
  sync_reader_->UpdatePendingBytes(kPauseMark);

  handler_->OnPaused(this);
}

void AudioOutputController::DoFlush() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // TODO(hclam): Actually flush the audio device.
}

void AudioOutputController::DoClose() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (state_ != kClosed) {
    DoStopCloseAndClearStream();
    sync_reader_->Close();
    state_ = kClosed;
  }
}

void AudioOutputController::DoSetVolume(double volume) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // Saves the volume to a member first. We may not be able to set the volume
  // right away but when the stream is created we'll set the volume.
  volume_ = volume;

  switch (state_) {
    case kCreated:
    case kStarting:
    case kPlaying:
    case kPaused:
      stream_->SetVolume(volume_);
      break;
    default:
      return;
  }
}

void AudioOutputController::DoReportError(int code) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  if (state_ != kClosed)
    handler_->OnError(this, code);
}

int AudioOutputController::OnMoreData(AudioBus* dest,
                                      AudioBuffersState buffers_state) {
  return OnMoreIOData(NULL, dest, buffers_state);
}

int AudioOutputController::OnMoreIOData(AudioBus* source,
                                        AudioBus* dest,
                                        AudioBuffersState buffers_state) {
  DisallowEntryToOnMoreIOData();
  TRACE_EVENT0("audio", "AudioOutputController::OnMoreIOData");

  const int frames = sync_reader_->Read(source, dest);
  DCHECK_LE(0, frames);
  sync_reader_->UpdatePendingBytes(
      buffers_state.total_bytes() + frames * params_.GetBytesPerFrame());

  AllowEntryToOnMoreIOData();
  return frames;
}

void AudioOutputController::WaitTillDataReady() {
#if defined(OS_WIN) || defined(OS_MACOSX)
  base::Time start = base::Time::Now();
  // Wait for up to 1.5 seconds for DataReady().  1.5 seconds was chosen because
  // it's larger than the playback time of the WaveOut buffer size using the
  // minimum supported sample rate: 4096 / 3000 = ~1.4 seconds.  Even a client
  // expecting real time playout should be able to fill in this time.
  const base::TimeDelta max_wait = base::TimeDelta::FromMilliseconds(1500);
  while (!sync_reader_->DataReady() &&
         ((base::Time::Now() - start) < max_wait)) {
    base::PlatformThread::YieldCurrentThread();
  }
#else
  // WaitTillDataReady() is deprecated and should not be used.
  CHECK(false);
#endif
}

void AudioOutputController::OnError(AudioOutputStream* stream, int code) {
  // Handle error on the audio controller thread.
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &AudioOutputController::DoReportError, this, code));
}

void AudioOutputController::DoStopCloseAndClearStream() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // Allow calling unconditionally and bail if we don't have a stream_ to close.
  if (stream_) {
    // De-register from state change callbacks if stream_ was created via
    // AudioManager.
    if (stream_ != diverting_to_stream_)
      audio_manager_->RemoveOutputDeviceChangeListener(this);

    StopStream();
    stream_->Close();
    if (stream_ == diverting_to_stream_)
      diverting_to_stream_ = NULL;
    stream_ = NULL;
  }

  state_ = kEmpty;
}

void AudioOutputController::OnDeviceChange() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // Recreate the stream (DoCreate() will first shut down an existing stream).
  // Exit if we ran into an error.
  const State original_state = state_;
  DoCreate(true);
  if (!stream_ || state_ == kError)
    return;

  // Get us back to the original state or an equivalent state.
  switch (original_state) {
    case kStarting:
    case kPlaying:
      DoPlay();
      return;
    case kCreated:
    case kPaused:
      // From the outside these two states are equivalent.
      return;
    default:
      NOTREACHED() << "Invalid original state.";
  }
}

const AudioParameters& AudioOutputController::GetAudioParameters() {
  return params_;
}

void AudioOutputController::StartDiverting(AudioOutputStream* to_stream) {
  message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&AudioOutputController::DoStartDiverting, this, to_stream));
}

void AudioOutputController::StopDiverting() {
  message_loop_->PostTask(
      FROM_HERE, base::Bind(&AudioOutputController::DoStopDiverting, this));
}

void AudioOutputController::DoStartDiverting(AudioOutputStream* to_stream) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (state_ == kClosed)
    return;

  DCHECK(!diverting_to_stream_);
  diverting_to_stream_ = to_stream;
  // Note: OnDeviceChange() will engage the "re-create" process, which will
  // detect and use the alternate AudioOutputStream rather than create a new one
  // via AudioManager.
  OnDeviceChange();
}

void AudioOutputController::DoStopDiverting() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (state_ == kClosed)
    return;

  // Note: OnDeviceChange() will cause the existing stream (the consumer of the
  // diverted audio data) to be closed, and diverting_to_stream_ will be set
  // back to NULL.
  OnDeviceChange();
  DCHECK(!diverting_to_stream_);
}

void AudioOutputController::AllowEntryToOnMoreIOData() {
  DCHECK(base::AtomicRefCountIsZero(&num_allowed_io_));
  base::AtomicRefCountInc(&num_allowed_io_);
}

void AudioOutputController::DisallowEntryToOnMoreIOData() {
  const bool is_zero = !base::AtomicRefCountDec(&num_allowed_io_);
  DCHECK(is_zero);
}

}  // namespace media
