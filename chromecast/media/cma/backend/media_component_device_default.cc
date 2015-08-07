// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/media_component_device_default.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/thread_task_runner_handle.h"
#include "chromecast/public/media/cast_decoder_buffer.h"
#include "chromecast/public/media/decrypt_context.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "chromecast/public/task_runner.h"
#include "media/base/buffers.h"

namespace chromecast {
namespace media {

namespace {

// Maximum number of frames that can be buffered.
const size_t kMaxFrameCount = 20;

// Wraps base::Closure in the chromecast/public TaskRunner interface.
class ClosureTask : public TaskRunner::Task {
 public:
  ClosureTask(const base::Closure& cb) : cb_(cb) {}
  void Run() override { cb_.Run(); }

 private:
  base::Closure cb_;
};

// Returns whether or not transitioning from |state1| to |state2| is valid.
inline static bool IsValidStateTransition(MediaComponentDevice::State state1,
                                          MediaComponentDevice::State state2) {
  if (state2 == state1)
    return true;

  // All states can transition to |kStateError|.
  if (state2 == MediaComponentDevice::kStateError)
    return true;

  // All the other valid FSM transitions.
  switch (state1) {
    case MediaComponentDevice::kStateUninitialized:
      return state2 == MediaComponentDevice::kStateIdle;
    case MediaComponentDevice::kStateIdle:
      return state2 == MediaComponentDevice::kStateRunning ||
             state2 == MediaComponentDevice::kStatePaused ||
             state2 == MediaComponentDevice::kStateUninitialized;
    case MediaComponentDevice::kStatePaused:
      return state2 == MediaComponentDevice::kStateIdle ||
             state2 == MediaComponentDevice::kStateRunning;
    case MediaComponentDevice::kStateRunning:
      return state2 == MediaComponentDevice::kStateIdle ||
             state2 == MediaComponentDevice::kStatePaused;
    case MediaComponentDevice::kStateError:
      return state2 == MediaComponentDevice::kStateUninitialized;

    default:
      return false;
  }
}

}  // namespace

MediaComponentDeviceDefault::DefaultDecoderBuffer::DefaultDecoderBuffer()
  : size(0) {
}

MediaComponentDeviceDefault::DefaultDecoderBuffer::~DefaultDecoderBuffer() {
}

MediaComponentDeviceDefault::MediaComponentDeviceDefault(
    const MediaPipelineDeviceParams& params,
    MediaClockDevice* media_clock_device)
    : task_runner_(params.task_runner),
      media_clock_device_(media_clock_device),
      state_(kStateUninitialized),
      rendering_time_(::media::kNoTimestamp()),
      decoded_frame_count_(0),
      decoded_byte_count_(0),
      scheduled_rendering_task_(false),
      weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
  thread_checker_.DetachFromThread();
}

MediaComponentDeviceDefault::~MediaComponentDeviceDefault() {
}

void MediaComponentDeviceDefault::SetClient(Client* client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  client_.reset(client);
}

MediaComponentDevice::State MediaComponentDeviceDefault::GetState() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return state_;
}

bool MediaComponentDeviceDefault::SetState(State new_state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsValidStateTransition(state_, new_state));
  state_ = new_state;

  if (state_ == kStateIdle) {
    // Back to the idle state: reset a bunch of parameters.
    is_eos_ = false;
    rendering_time_ = ::media::kNoTimestamp();
    decoded_frame_count_ = 0;
    decoded_byte_count_ = 0;
    frames_.clear();
    pending_buffer_ = scoped_ptr<CastDecoderBuffer>();
    frame_pushed_cb_.reset();
    return true;
  }

  if (state_ == kStateRunning) {
    if (!scheduled_rendering_task_) {
      scheduled_rendering_task_ = true;
      task_runner_->PostTask(
          new ClosureTask(
              base::Bind(&MediaComponentDeviceDefault::RenderTask, weak_this_)),
          0);
    }
    return true;
  }

  return true;
}

bool MediaComponentDeviceDefault::SetStartPts(int64_t time_microseconds) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, kStateIdle);
  rendering_time_ = base::TimeDelta::FromMicroseconds(time_microseconds);
  return true;
}

MediaComponentDevice::FrameStatus MediaComponentDeviceDefault::PushFrame(
    DecryptContext* decrypt_context_in,
    CastDecoderBuffer* buffer_in,
    FrameStatusCB* completion_cb_in) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(state_ == kStatePaused || state_ == kStateRunning);
  DCHECK(!is_eos_);
  DCHECK(!pending_buffer_.get());
  DCHECK(buffer_in);

  scoped_ptr<DecryptContext> decrypt_context(decrypt_context_in);
  scoped_ptr<FrameStatusCB> completion_cb(completion_cb_in);

  scoped_ptr<CastDecoderBuffer> buffer(buffer_in);
  if (buffer->end_of_stream()) {
    is_eos_ = true;
    return kFrameSuccess;
  }

  if (frames_.size() > kMaxFrameCount) {
    pending_buffer_ = buffer.Pass();
    frame_pushed_cb_ = completion_cb.Pass();
    return kFramePending;
  }

  DefaultDecoderBuffer fake_buffer;
  fake_buffer.size = buffer->data_size();
  fake_buffer.pts = base::TimeDelta::FromMicroseconds(buffer->timestamp());
  frames_.push_back(fake_buffer);
  return kFrameSuccess;
}

MediaComponentDeviceDefault::RenderingDelay
MediaComponentDeviceDefault::GetRenderingDelay() const {
  NOTIMPLEMENTED();
  return RenderingDelay();
}

void MediaComponentDeviceDefault::RenderTask() {
  scheduled_rendering_task_ = false;

  if (state_ != kStateRunning)
    return;

  base::TimeDelta media_time = base::TimeDelta::FromMicroseconds(
      media_clock_device_->GetTimeMicroseconds());
  if (media_time == ::media::kNoTimestamp()) {
    scheduled_rendering_task_ = true;
    task_runner_->PostTask(
        new ClosureTask(
            base::Bind(&MediaComponentDeviceDefault::RenderTask, weak_this_)),
        50);
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
      fake_buffer.pts =
          base::TimeDelta::FromMicroseconds(pending_buffer_->timestamp());
      frames_.push_back(fake_buffer);
      pending_buffer_ = scoped_ptr<CastDecoderBuffer>();
      frame_pushed_cb_->Run(kFrameSuccess);
      frame_pushed_cb_.reset();
    }
  }

  if (frames_.empty() && is_eos_) {
    if (client_) {
      client_->OnEndOfStream();
    }
    return;
  }

  scheduled_rendering_task_ = true;
  task_runner_->PostTask(
      new ClosureTask(
          base::Bind(&MediaComponentDeviceDefault::RenderTask, weak_this_)),
      50);
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
