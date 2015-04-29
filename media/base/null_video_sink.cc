// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/null_video_sink.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"

namespace media {

NullVideoSink::NullVideoSink(
    bool clockless,
    base::TimeDelta interval,
    const NewFrameCB& new_frame_cb,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : clockless_(clockless),
      interval_(interval),
      new_frame_cb_(new_frame_cb),
      task_runner_(task_runner),
      started_(false),
      callback_(nullptr),
      tick_clock_(&default_tick_clock_) {
}

NullVideoSink::~NullVideoSink() {
  DCHECK(!started_);
}

void NullVideoSink::Start(RenderCallback* callback) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!started_);
  callback_ = callback;
  started_ = true;
  last_now_ = current_render_time_ = tick_clock_->NowTicks();
  cancelable_worker_.Reset(
      base::Bind(&NullVideoSink::CallRender, base::Unretained(this)));
  task_runner_->PostTask(FROM_HERE, cancelable_worker_.callback());
}

void NullVideoSink::Stop() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  cancelable_worker_.Cancel();
  started_ = false;
  if (!stop_cb_.is_null())
    base::ResetAndReturn(&stop_cb_).Run();
}

void NullVideoSink::CallRender() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(started_);

  const base::TimeTicks end_of_interval = current_render_time_ + interval_;
  if (current_render_time_ > pause_end_time_) {
    scoped_refptr<VideoFrame> new_frame =
        callback_->Render(current_render_time_, end_of_interval);
    const bool is_new_frame = new_frame != last_frame_;
    last_frame_ = new_frame;
    if (is_new_frame)
      new_frame_cb_.Run(new_frame);
  }

  current_render_time_ += interval_;

  if (clockless_) {
    task_runner_->PostTask(FROM_HERE, cancelable_worker_.callback());
    return;
  }

  // Recompute now to compensate for the cost of Render().
  const base::TimeTicks now = tick_clock_->NowTicks();
  base::TimeDelta delay = current_render_time_ - now;

  // If we're behind, find the next nearest on time interval.
  if (delay < base::TimeDelta())
    delay += interval_ * (-delay / interval_ + 1);
  current_render_time_ = now + delay;

  // The tick clock is frozen in this case, so clamp delay to the interval time.
  // We still want the interval passed to Render() to grow, but we also don't
  // want the delay used here to increase slowly over time.
  if (last_now_ == now && delay > interval_)
    delay = interval_;
  last_now_ = now;

  task_runner_->PostDelayedTask(FROM_HERE, cancelable_worker_.callback(),
                                delay);
}

void NullVideoSink::PaintFrameUsingOldRenderingPath(
    const scoped_refptr<VideoFrame>& frame) {
  new_frame_cb_.Run(frame);
}

void NullVideoSink::PauseRenderCallbacks(base::TimeTicks pause_until) {
  pause_end_time_ = pause_until;
}

}  // namespace media
