// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/media_component_device_default.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/thread_task_runner_handle.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"
#include "media/base/buffers.h"

namespace chromecast {
namespace media {

namespace {

// Maximum number of frames that can be buffered.
const size_t kMaxFrameCount = 20;

}  // namespace

MediaComponentDeviceDefault::DefaultDecoderBuffer::DefaultDecoderBuffer()
  : size(0) {
}

MediaComponentDeviceDefault::DefaultDecoderBuffer::~DefaultDecoderBuffer() {
}

MediaComponentDeviceDefault::MediaComponentDeviceDefault(
    MediaClockDevice* media_clock_device)
    : media_clock_device_(media_clock_device),
      state_(kStateUninitialized),
      rendering_time_(::media::kNoTimestamp()),
      decoded_frame_count_(0),
      decoded_byte_count_(0),
      scheduled_rendering_task_(false),
      weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
  DetachFromThread();
}

MediaComponentDeviceDefault::~MediaComponentDeviceDefault() {
}

void MediaComponentDeviceDefault::SetClient(const Client& client) {
  DCHECK(CalledOnValidThread());
  client_ = client;
}

MediaComponentDevice::State MediaComponentDeviceDefault::GetState() const {
  DCHECK(CalledOnValidThread());
  return state_;
}

bool MediaComponentDeviceDefault::SetState(State new_state) {
  DCHECK(CalledOnValidThread());
  if (!MediaComponentDevice::IsValidStateTransition(state_, new_state))
    return false;
  state_ = new_state;

  if (state_ == kStateIdle) {
    // Back to the idle state: reset a bunch of parameters.
    is_eos_ = false;
    rendering_time_ = ::media::kNoTimestamp();
    decoded_frame_count_ = 0;
    decoded_byte_count_ = 0;
    frames_.clear();
    pending_buffer_ = scoped_refptr<DecoderBufferBase>();
    frame_pushed_cb_.Reset();
    return true;
  }

  if (state_ == kStateRunning) {
    if (!scheduled_rendering_task_) {
      scheduled_rendering_task_ = true;
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(&MediaComponentDeviceDefault::RenderTask, weak_this_));
    }
    return true;
  }

  return true;
}

bool MediaComponentDeviceDefault::SetStartPts(base::TimeDelta time) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(state_, kStateIdle);
  rendering_time_ = time;
  return true;
}

MediaComponentDevice::FrameStatus MediaComponentDeviceDefault::PushFrame(
    const scoped_refptr<DecryptContext>& decrypt_context,
    const scoped_refptr<DecoderBufferBase>& buffer,
    const FrameStatusCB& completion_cb) {
  DCHECK(CalledOnValidThread());
  DCHECK(state_ == kStatePaused || state_ == kStateRunning);
  DCHECK(!is_eos_);
  DCHECK(!pending_buffer_.get());
  DCHECK(buffer.get());

  if (buffer->end_of_stream()) {
    is_eos_ = true;
    return kFrameSuccess;
  }

  if (frames_.size() > kMaxFrameCount) {
    pending_buffer_ = buffer;
    frame_pushed_cb_ = completion_cb;
    return kFramePending;
  }

  DefaultDecoderBuffer fake_buffer;
  fake_buffer.size = buffer->data_size();
  fake_buffer.pts = buffer->timestamp();
  frames_.push_back(fake_buffer);
  return kFrameSuccess;
}

base::TimeDelta MediaComponentDeviceDefault::GetRenderingTime() const {
  return rendering_time_;
}

base::TimeDelta MediaComponentDeviceDefault::GetRenderingDelay() const {
  NOTIMPLEMENTED();
  return ::media::kNoTimestamp();
}

void MediaComponentDeviceDefault::RenderTask() {
  scheduled_rendering_task_ = false;

  if (state_ != kStateRunning)
    return;

  base::TimeDelta media_time = media_clock_device_->GetTime();
  if (media_time == ::media::kNoTimestamp()) {
    scheduled_rendering_task_ = true;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&MediaComponentDeviceDefault::RenderTask, weak_this_),
        base::TimeDelta::FromMilliseconds(50));
    return;
  }

  while (!frames_.empty() && frames_.front().pts <= media_time) {
    rendering_time_ = frames_.front().pts;
    decoded_frame_count_++;
    decoded_byte_count_ += frames_.front().size;
    frames_.pop_front();
    if (pending_buffer_.get()) {
      DefaultDecoderBuffer fake_buffer;
      fake_buffer.size = pending_buffer_->data_size();
      fake_buffer.pts = pending_buffer_->timestamp();
      frames_.push_back(fake_buffer);
      pending_buffer_ = scoped_refptr<DecoderBufferBase>();
      base::ResetAndReturn(&frame_pushed_cb_).Run(kFrameSuccess);
    }
  }

  if (frames_.empty() && is_eos_) {
    if (!client_.eos_cb.is_null())
      client_.eos_cb.Run();
    return;
  }

  scheduled_rendering_task_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&MediaComponentDeviceDefault::RenderTask, weak_this_),
      base::TimeDelta::FromMilliseconds(50));
}

bool MediaComponentDeviceDefault::GetStatistics(Statistics* stats) const {
  if (state_ != kStateRunning)
    return false;

  // Note: what is returned here is not the number of samples but the number of
  // frames. The value is different for audio.
  stats->decoded_bytes = decoded_byte_count_;
  stats->decoded_samples = decoded_frame_count_;
  stats->dropped_samples = 0;
  return true;
}

}  // namespace media
}  // namespace chromecast
