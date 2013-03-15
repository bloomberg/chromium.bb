// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/fake_audio_output_stream.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "media/audio/audio_manager_base.h"

namespace media {

// static
AudioOutputStream* FakeAudioOutputStream::MakeFakeStream(
    AudioManagerBase* manager, const AudioParameters& params) {
  return new FakeAudioOutputStream(manager, params);
}

FakeAudioOutputStream::FakeAudioOutputStream(AudioManagerBase* manager,
                                             const AudioParameters& params)
    : audio_manager_(manager),
      callback_(NULL),
      audio_bus_(AudioBus::Create(params)),
      buffer_duration_(base::TimeDelta::FromMicroseconds(
          params.frames_per_buffer() * base::Time::kMicrosecondsPerSecond /
          static_cast<float>(params.sample_rate()))) {
  audio_bus_->Zero();
}

FakeAudioOutputStream::~FakeAudioOutputStream() {
  DCHECK(!callback_);
}

bool FakeAudioOutputStream::Open() {
  DCHECK(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());
  return true;
}

void FakeAudioOutputStream::Start(AudioSourceCallback* callback)  {
  DCHECK(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());
  callback_ = callback;
  next_read_time_ = base::Time::Now();
  on_more_data_cb_.Reset(base::Bind(
      &FakeAudioOutputStream::OnMoreDataTask, base::Unretained(this)));
  audio_manager_->GetMessageLoop()->PostTask(
      FROM_HERE, on_more_data_cb_.callback());
}

void FakeAudioOutputStream::Stop() {
  DCHECK(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());
  callback_ = NULL;
  on_more_data_cb_.Cancel();
}

void FakeAudioOutputStream::Close() {
  DCHECK(!callback_);
  DCHECK(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());
  audio_manager_->ReleaseOutputStream(this);
}

void FakeAudioOutputStream::SetVolume(double volume) {};

void FakeAudioOutputStream::GetVolume(double* volume) {
  *volume = 0;
};

void FakeAudioOutputStream::OnMoreDataTask() {
  DCHECK(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());
  DCHECK(callback_);

  callback_->OnMoreData(audio_bus_.get(), AudioBuffersState());

  // Need to account for time spent here due to the cost of OnMoreData() as well
  // as the imprecision of PostDelayedTask().
  base::Time now = base::Time::Now();
  base::TimeDelta delay = next_read_time_ + buffer_duration_ - now;

  // If we're behind, find the next nearest ontime interval.
  if (delay < base::TimeDelta())
    delay += buffer_duration_ * (-delay / buffer_duration_ + 1);
  next_read_time_ = now + delay;

  audio_manager_->GetMessageLoop()->PostDelayedTask(
      FROM_HERE, on_more_data_cb_.callback(), delay);
}

}  // namespace media
