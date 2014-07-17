// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/fake_audio_input_stream.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "media/audio/audio_manager_base.h"
#include "media/base/audio_bus.h"

using base::TimeTicks;
using base::TimeDelta;

namespace media {

namespace {

// These values are based on experiments for local-to-local
// PeerConnection to demonstrate audio/video synchronization.
const int kBeepDurationMilliseconds = 20;
const int kBeepFrequency = 400;

// Intervals between two automatic beeps.
const int kAutomaticBeepIntervalInMs = 500;

// Automatic beep will be triggered every |kAutomaticBeepIntervalInMs| unless
// users explicitly call BeepOnce(), which will disable the automatic beep.
class BeepContext {
 public:
  BeepContext() : beep_once_(false), automatic_beep_(true) {}

  void SetBeepOnce(bool enable) {
    base::AutoLock auto_lock(lock_);
    beep_once_ = enable;

    // Disable the automatic beep if users explicit set |beep_once_| to true.
    if (enable)
      automatic_beep_ = false;
  }
  bool beep_once() const {
    base::AutoLock auto_lock(lock_);
    return beep_once_;
  }
  bool automatic_beep() const {
    base::AutoLock auto_lock(lock_);
    return automatic_beep_;
  }

 private:
  mutable base::Lock lock_;
  bool beep_once_;
  bool automatic_beep_;
};

static base::LazyInstance<BeepContext> g_beep_context =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

AudioInputStream* FakeAudioInputStream::MakeFakeStream(
    AudioManagerBase* manager,
    const AudioParameters& params) {
  return new FakeAudioInputStream(manager, params);
}

FakeAudioInputStream::FakeAudioInputStream(AudioManagerBase* manager,
                                           const AudioParameters& params)
    : audio_manager_(manager),
      callback_(NULL),
      buffer_size_((params.channels() * params.bits_per_sample() *
                    params.frames_per_buffer()) /
                   8),
      params_(params),
      task_runner_(manager->GetTaskRunner()),
      callback_interval_(base::TimeDelta::FromMilliseconds(
          (params.frames_per_buffer() * 1000) / params.sample_rate())),
      beep_duration_in_buffers_(kBeepDurationMilliseconds *
                                params.sample_rate() /
                                params.frames_per_buffer() /
                                1000),
      beep_generated_in_buffers_(0),
      beep_period_in_frames_(params.sample_rate() / kBeepFrequency),
      frames_elapsed_(0),
      audio_bus_(AudioBus::Create(params)),
      weak_factory_(this) {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
}

FakeAudioInputStream::~FakeAudioInputStream() {}

bool FakeAudioInputStream::Open() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  buffer_.reset(new uint8[buffer_size_]);
  memset(buffer_.get(), 0, buffer_size_);
  audio_bus_->Zero();
  return true;
}

void FakeAudioInputStream::Start(AudioInputCallback* callback)  {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(!callback_);
  callback_ = callback;
  last_callback_time_ = TimeTicks::Now();
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeAudioInputStream::DoCallback, weak_factory_.GetWeakPtr()),
      callback_interval_);
}

void FakeAudioInputStream::DoCallback() {
  DCHECK(callback_);

  const TimeTicks now = TimeTicks::Now();
  base::TimeDelta next_callback_time =
      last_callback_time_ + callback_interval_ * 2 - now;

  // If we are falling behind, try to catch up as much as we can in the next
  // callback.
  if (next_callback_time < base::TimeDelta())
    next_callback_time = base::TimeDelta();

  // Accumulate the time from the last beep.
  interval_from_last_beep_ += now - last_callback_time_;

  last_callback_time_ = now;

  memset(buffer_.get(), 0, buffer_size_);

  bool should_beep = false;
  {
    BeepContext* beep_context = g_beep_context.Pointer();
    if (beep_context->automatic_beep()) {
      base::TimeDelta delta = interval_from_last_beep_ -
          TimeDelta::FromMilliseconds(kAutomaticBeepIntervalInMs);
      if (delta > base::TimeDelta()) {
        should_beep = true;
        interval_from_last_beep_ = delta;
      }
    } else {
      should_beep = beep_context->beep_once();
      beep_context->SetBeepOnce(false);
    }
  }

  // If this object was instructed to generate a beep or has started to
  // generate a beep sound.
  if (should_beep || beep_generated_in_buffers_) {
    // Compute the number of frames to output high value. Then compute the
    // number of bytes based on channels and bits per channel.
    int high_frames = beep_period_in_frames_ / 2;
    int high_bytes = high_frames * params_.bits_per_sample() *
        params_.channels() / 8;

    // Separate high and low with the same number of bytes to generate a
    // square wave.
    int position = 0;
    while (position + high_bytes <= buffer_size_) {
      // Write high values first.
      memset(buffer_.get() + position, 128, high_bytes);
      // Then leave low values in the buffer with |high_bytes|.
      position += high_bytes * 2;
    }

    ++beep_generated_in_buffers_;
    if (beep_generated_in_buffers_ >= beep_duration_in_buffers_)
      beep_generated_in_buffers_ = 0;
  }

  audio_bus_->FromInterleaved(
      buffer_.get(), audio_bus_->frames(), params_.bits_per_sample() / 8);
  callback_->OnData(this, audio_bus_.get(), buffer_size_, 1.0);
  frames_elapsed_ += params_.frames_per_buffer();

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeAudioInputStream::DoCallback, weak_factory_.GetWeakPtr()),
      next_callback_time);
}

void FakeAudioInputStream::Stop() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  weak_factory_.InvalidateWeakPtrs();
  callback_ = NULL;
}

void FakeAudioInputStream::Close() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  audio_manager_->ReleaseInputStream(this);
}

double FakeAudioInputStream::GetMaxVolume() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  return 1.0;
}

void FakeAudioInputStream::SetVolume(double volume) {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
}

double FakeAudioInputStream::GetVolume() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  return 1.0;
}

void FakeAudioInputStream::SetAutomaticGainControl(bool enabled) {}

bool FakeAudioInputStream::GetAutomaticGainControl() {
  return true;
}

// static
void FakeAudioInputStream::BeepOnce() {
  BeepContext* beep_context = g_beep_context.Pointer();
  beep_context->SetBeepOnce(true);
}

}  // namespace media
