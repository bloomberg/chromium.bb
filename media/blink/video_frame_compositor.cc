// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/video_frame_compositor.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "media/base/video_frame.h"

namespace media {

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
      natural_size_changed_cb_(natural_size_changed_cb),
      opacity_changed_cb_(opacity_changed_cb),
      client_(nullptr),
      rendering_(false),
      callback_(nullptr) {
}

VideoFrameCompositor::~VideoFrameCompositor() {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  DCHECK(!callback_);
  DCHECK(!rendering_);
  if (client_)
    client_->StopUsingProvider();
}

void VideoFrameCompositor::OnRendererStateUpdate() {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  if (!client_)
    return;

  base::AutoLock lock(lock_);
  if (callback_) {
    if (rendering_)
      client_->StartRendering();

    // TODO(dalecurtis): This will need to request the first frame so we have
    // something to show, even if playback hasn't started yet.
  } else if (rendering_) {
    client_->StopRendering();
  }
}

scoped_refptr<VideoFrame>
VideoFrameCompositor::GetCurrentFrameAndUpdateIfStale() {
  // TODO(dalecurtis): Implement frame refresh when stale.
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  return GetCurrentFrame();
}

void VideoFrameCompositor::SetVideoFrameProviderClient(
    cc::VideoFrameProvider::Client* client) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  if (client_)
    client_->StopUsingProvider();
  client_ = client;
  OnRendererStateUpdate();
}

scoped_refptr<VideoFrame> VideoFrameCompositor::GetCurrentFrame() {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  return current_frame_;
}

void VideoFrameCompositor::PutCurrentFrame() {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  // TODO(dalecurtis): Wire up a flag for RenderCallback::OnFrameDropped().
}

bool VideoFrameCompositor::UpdateCurrentFrame(base::TimeTicks deadline_min,
                                              base::TimeTicks deadline_max) {
  // TODO(dalecurtis): Wire this up to RenderCallback::Render().
  base::AutoLock lock(lock_);
  return false;
}

void VideoFrameCompositor::Start(RenderCallback* callback) {
  NOTREACHED();

  // Called from the media thread, so acquire the callback under lock before
  // returning in case a Stop() call comes in before the PostTask is processed.
  base::AutoLock lock(lock_);
  callback_ = callback;
  rendering_ = true;
  compositor_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoFrameCompositor::OnRendererStateUpdate,
                            base::Unretained(this)));
}

void VideoFrameCompositor::Stop() {
  NOTREACHED();

  // Called from the media thread, so release the callback under lock before
  // returning to avoid a pending UpdateCurrentFrame() call occurring before
  // the PostTask is processed.
  base::AutoLock lock(lock_);
  callback_ = nullptr;
  rendering_ = false;
  compositor_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoFrameCompositor::OnRendererStateUpdate,
                            base::Unretained(this)));
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

  if (current_frame_.get() &&
      current_frame_->natural_size() != frame->natural_size()) {
    natural_size_changed_cb_.Run(frame->natural_size());
  }

  if (!current_frame_.get() || IsOpaque(current_frame_) != IsOpaque(frame)) {
    opacity_changed_cb_.Run(IsOpaque(frame));
  }

  current_frame_ = frame;

  if (client_)
    client_->DidReceiveFrame();
}

}  // namespace media
