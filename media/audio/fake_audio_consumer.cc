// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/fake_audio_consumer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "media/base/audio_bus.h"

namespace media {

FakeAudioConsumer::FakeAudioConsumer(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    const AudioParameters& params)
    : message_loop_(message_loop),
      audio_bus_(AudioBus::Create(params)),
      buffer_duration_(base::TimeDelta::FromMicroseconds(
          params.frames_per_buffer() * base::Time::kMicrosecondsPerSecond /
          static_cast<float>(params.sample_rate()))) {
  audio_bus_->Zero();
}

FakeAudioConsumer::~FakeAudioConsumer() {
  DCHECK(read_cb_.is_null());
}

void FakeAudioConsumer::Start(const ReadCB& read_cb)  {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(read_cb_.is_null());
  DCHECK(!read_cb.is_null());
  read_cb_ = read_cb;
  next_read_time_ = base::Time::Now();
  read_task_cb_.Reset(base::Bind(
      &FakeAudioConsumer::DoRead, base::Unretained(this)));
  message_loop_->PostTask(FROM_HERE, read_task_cb_.callback());
}

void FakeAudioConsumer::Stop() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  read_cb_.Reset();
  read_task_cb_.Cancel();
}

void FakeAudioConsumer::DoRead() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(!read_cb_.is_null());

  read_cb_.Run(audio_bus_.get());

  // Need to account for time spent here due to the cost of |read_cb_| as well
  // as the imprecision of PostDelayedTask().
  base::Time now = base::Time::Now();
  base::TimeDelta delay = next_read_time_ + buffer_duration_ - now;

  // If we're behind, find the next nearest ontime interval.
  if (delay < base::TimeDelta())
    delay += buffer_duration_ * (-delay / buffer_duration_ + 1);
  next_read_time_ = now + delay;

  message_loop_->PostDelayedTask(FROM_HERE, read_task_cb_.callback(), delay);
}

}  // namespace media
