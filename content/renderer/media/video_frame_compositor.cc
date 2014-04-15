// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/video_frame_compositor.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "cc/layers/video_frame_provider.h"
#include "content/renderer/render_thread_impl.h"
#include "media/base/video_frame.h"

namespace content {

static bool IsOpaque(const scoped_refptr<media::VideoFrame>& frame) {
  switch (frame->format()) {
    case media::VideoFrame::UNKNOWN:
    case media::VideoFrame::YV12:
    case media::VideoFrame::YV12J:
    case media::VideoFrame::YV16:
    case media::VideoFrame::I420:
      return true;

    case media::VideoFrame::YV12A:
#if defined(VIDEO_HOLE)
    case media::VideoFrame::HOLE:
#endif  // defined(VIDEO_HOLE)
    case media::VideoFrame::NATIVE_TEXTURE:
      break;
  }
  return false;
}

class VideoFrameCompositor::Internal : public cc::VideoFrameProvider {
 public:
  Internal(
      const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner,
      const base::Callback<void(gfx::Size)>& natural_size_changed_cb,
      const base::Callback<void(bool)>& opacity_changed_cb)
      : compositor_task_runner_(compositor_task_runner),
        natural_size_changed_cb_(natural_size_changed_cb),
        opacity_changed_cb_(opacity_changed_cb),
        client_(NULL),
        compositor_notification_pending_(false),
        frames_dropped_before_compositor_was_notified_(0) {}

  virtual ~Internal() {
    if (client_)
      client_->StopUsingProvider();
  }

  void DeleteSoon() {
    compositor_task_runner_->DeleteSoon(FROM_HERE, this);
  }

  void UpdateCurrentFrame(const scoped_refptr<media::VideoFrame>& frame) {
    base::AutoLock auto_lock(lock_);

    if (current_frame_ &&
        current_frame_->natural_size() != frame->natural_size()) {
      natural_size_changed_cb_.Run(frame->natural_size());
    }

    if (!current_frame_ || IsOpaque(current_frame_) != IsOpaque(frame)) {
      opacity_changed_cb_.Run(IsOpaque(frame));
    }

    current_frame_ = frame;

    // Count frames as dropped if and only if we updated the frame but didn't
    // finish notifying the compositor for the previous frame.
    if (compositor_notification_pending_) {
      if (frames_dropped_before_compositor_was_notified_ < kuint32max)
        ++frames_dropped_before_compositor_was_notified_;
      return;
    }

    compositor_notification_pending_ = true;
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&Internal::NotifyCompositorOfNewFrame,
                   base::Unretained(this)));
  }

  uint32 GetFramesDroppedBeforeCompositorWasNotified() {
    base::AutoLock auto_lock(lock_);
    return frames_dropped_before_compositor_was_notified_;
  }

  void SetFramesDroppedBeforeCompositorWasNotifiedForTesting(
      uint32 dropped_frames) {
    base::AutoLock auto_lock(lock_);
    frames_dropped_before_compositor_was_notified_ = dropped_frames;
  }

  // cc::VideoFrameProvider implementation.
  virtual void SetVideoFrameProviderClient(
      cc::VideoFrameProvider::Client* client) OVERRIDE {
    if (client_)
      client_->StopUsingProvider();
    client_ = client;
  }

  virtual scoped_refptr<media::VideoFrame> GetCurrentFrame() OVERRIDE {
    base::AutoLock auto_lock(lock_);
    return current_frame_;
  }

  virtual void PutCurrentFrame(const scoped_refptr<media::VideoFrame>& frame)
      OVERRIDE {}

 private:
  void NotifyCompositorOfNewFrame() {
    base::AutoLock auto_lock(lock_);
    compositor_notification_pending_ = false;
    if (client_)
      client_->DidReceiveFrame();
  }

  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  base::Callback<void(gfx::Size)> natural_size_changed_cb_;
  base::Callback<void(bool)> opacity_changed_cb_;

  cc::VideoFrameProvider::Client* client_;

  base::Lock lock_;
  scoped_refptr<media::VideoFrame> current_frame_;
  bool compositor_notification_pending_;
  uint32 frames_dropped_before_compositor_was_notified_;

  DISALLOW_COPY_AND_ASSIGN(Internal);
};

VideoFrameCompositor::VideoFrameCompositor(
    const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner,
    const base::Callback<void(gfx::Size)>& natural_size_changed_cb,
    const base::Callback<void(bool)>& opacity_changed_cb)
    : internal_(new Internal(compositor_task_runner,
                             natural_size_changed_cb,
                             opacity_changed_cb)) {
}

VideoFrameCompositor::~VideoFrameCompositor() {
  internal_->DeleteSoon();
}

cc::VideoFrameProvider* VideoFrameCompositor::GetVideoFrameProvider() {
  return internal_;
}

void VideoFrameCompositor::UpdateCurrentFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
  internal_->UpdateCurrentFrame(frame);
}

scoped_refptr<media::VideoFrame> VideoFrameCompositor::GetCurrentFrame() {
  return internal_->GetCurrentFrame();
}

uint32 VideoFrameCompositor::GetFramesDroppedBeforeCompositorWasNotified() {
  return internal_->GetFramesDroppedBeforeCompositorWasNotified();
}

void
VideoFrameCompositor::SetFramesDroppedBeforeCompositorWasNotifiedForTesting(
    uint32 dropped_frames) {
  internal_->SetFramesDroppedBeforeCompositorWasNotifiedForTesting(
      dropped_frames);
}

}  // namespace content
