// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/video_frame_compositor.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "media/base/video_frame.h"

namespace media {

// The maximum time we'll allow to elapse between Render() callbacks if there is
// an external caller requesting frames via GetCurrentFrame(); i.e. there is a
// canvas which frames are being copied into.
const int kStaleFrameThresholdMs = 250;

static bool IsOpaque(const scoped_refptr<VideoFrame>& frame) {
  switch (frame->format()) {
    case VideoFrame::UNKNOWN:
    case VideoFrame::YV12:
    case VideoFrame::YV12J:
    case VideoFrame::YV12HD:
    case VideoFrame::YV16:
    case VideoFrame::I420:
    case VideoFrame::YV24:
    case VideoFrame::NV12:
      return true;

    case VideoFrame::YV12A:
#if defined(VIDEO_HOLE)
    case VideoFrame::HOLE:
#endif  // defined(VIDEO_HOLE)
    case VideoFrame::NATIVE_TEXTURE:
    case VideoFrame::ARGB:
      break;
  }
  return false;
}

VideoFrameCompositor::VideoFrameCompositor(
    const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner,
    const base::Callback<void(gfx::Size)>& natural_size_changed_cb,
    const base::Callback<void(bool)>& opacity_changed_cb)
    : compositor_task_runner_(compositor_task_runner),
      tick_clock_(new base::DefaultTickClock()),
      natural_size_changed_cb_(natural_size_changed_cb),
      opacity_changed_cb_(opacity_changed_cb),
      stale_frame_threshold_(
          base::TimeDelta::FromMilliseconds(kStaleFrameThresholdMs)),
      client_(nullptr),
      rendering_(false),
      rendered_last_frame_(false),
      callback_(nullptr),
      // Assume 60Hz before the first UpdateCurrentFrame() call.
      last_interval_(base::TimeDelta::FromSecondsD(1.0 / 60)) {
}

VideoFrameCompositor::~VideoFrameCompositor() {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  DCHECK(!callback_);
  DCHECK(!rendering_);
  if (client_)
    client_->StopUsingProvider();
}

void VideoFrameCompositor::OnRendererStateUpdate(bool new_state) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(rendering_, new_state);
  rendering_ = new_state;
  last_frame_update_time_ = base::TimeTicks();

  if (!client_)
    return;

  if (rendering_)
    client_->StartRendering();
  else
    client_->StopRendering();
}

scoped_refptr<VideoFrame>
VideoFrameCompositor::GetCurrentFrameAndUpdateIfStale() {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  if (rendering_) {
    const base::TimeTicks now = tick_clock_->NowTicks();
    if (now - last_frame_update_time_ > stale_frame_threshold_)
      UpdateCurrentFrame(now, now + last_interval_);
  }

  return GetCurrentFrame();
}

void VideoFrameCompositor::SetVideoFrameProviderClient(
    cc::VideoFrameProvider::Client* client) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  if (client_)
    client_->StopUsingProvider();
  client_ = client;

  if (rendering_)
    client_->StartRendering();
}

scoped_refptr<VideoFrame> VideoFrameCompositor::GetCurrentFrame() {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  return current_frame_;
}

void VideoFrameCompositor::PutCurrentFrame() {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  rendered_last_frame_ = true;
}

bool VideoFrameCompositor::UpdateCurrentFrame(base::TimeTicks deadline_min,
                                              base::TimeTicks deadline_max) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(lock_);
  if (!callback_)
    return false;

  DCHECK(rendering_);

  // If the previous frame was never rendered, let the client know.
  if (!rendered_last_frame_ && current_frame_)
    callback_->OnFrameDropped();

  last_frame_update_time_ = tick_clock_->NowTicks();
  last_interval_ = deadline_max - deadline_min;
  return ProcessNewFrame(callback_->Render(deadline_min, deadline_max), false);
}

void VideoFrameCompositor::Start(RenderCallback* callback) {
  TRACE_EVENT0("media", __FUNCTION__);

  // Called from the media thread, so acquire the callback under lock before
  // returning in case a Stop() call comes in before the PostTask is processed.
  base::AutoLock lock(lock_);
  DCHECK(!callback_);
  callback_ = callback;
  compositor_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoFrameCompositor::OnRendererStateUpdate,
                            base::Unretained(this), true));
}

void VideoFrameCompositor::Stop() {
  TRACE_EVENT0("media", __FUNCTION__);

  // Called from the media thread, so release the callback under lock before
  // returning to avoid a pending UpdateCurrentFrame() call occurring before
  // the PostTask is processed.
  base::AutoLock lock(lock_);
  DCHECK(callback_);

  // Fire one more Render() callback if we're more than one render interval away
  // to ensure that we have a good frame to display if Render() has never been
  // called, or was called long enough ago that the frame is stale.  We must
  // always have a |current_frame_| to recover from "damage" to the video layer.
  const base::TimeTicks now = tick_clock_->NowTicks();
  if (now - last_frame_update_time_ > last_interval_) {
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&VideoFrameCompositor::ProcessNewFrame),
                   base::Unretained(this),
                   callback_->Render(now, now + last_interval_), true));
  }

  callback_ = nullptr;
  compositor_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoFrameCompositor::OnRendererStateUpdate,
                            base::Unretained(this), false));
}

void VideoFrameCompositor::PaintFrameUsingOldRenderingPath(
    const scoped_refptr<VideoFrame>& frame) {
  if (!compositor_task_runner_->BelongsToCurrentThread()) {
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&VideoFrameCompositor::PaintFrameUsingOldRenderingPath,
                   base::Unretained(this), frame));
    return;
  }

  ProcessNewFrame(frame, true);
}

bool VideoFrameCompositor::ProcessNewFrame(
    const scoped_refptr<VideoFrame>& frame,
    bool notify_client_of_new_frames) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());

  if (frame == current_frame_)
    return false;

  // Set the flag indicating that the current frame is unrendered, if we get a
  // subsequent PutCurrentFrame() call it will mark it as rendered.
  rendered_last_frame_ = false;

  if (current_frame_ &&
      current_frame_->natural_size() != frame->natural_size()) {
    natural_size_changed_cb_.Run(frame->natural_size());
  }

  if (!current_frame_ || IsOpaque(current_frame_) != IsOpaque(frame))
    opacity_changed_cb_.Run(IsOpaque(frame));

  current_frame_ = frame;
  if (notify_client_of_new_frames && client_)
    client_->DidReceiveFrame();

  return true;
}

}  // namespace media
