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

class VideoFrameCompositor::Internal : public cc::VideoFrameProvider {
 public:
  Internal(
      const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner,
      const base::Callback<void(gfx::Size)>& natural_size_changed_cb)
      : compositor_task_runner_(compositor_task_runner),
        natural_size_changed_cb_(natural_size_changed_cb),
        client_(NULL),
        compositor_notify_finished_(true),
        current_frame_composited_(false),
        frames_dropped_before_composite_(0) {}

  virtual ~Internal() {
    if (client_)
      client_->StopUsingProvider();
  }

  void DeleteSoon() {
    compositor_task_runner_->DeleteSoon(FROM_HERE, this);
  }

  void UpdateCurrentFrame(const scoped_refptr<media::VideoFrame>& frame) {
    base::AutoLock auto_lock(lock_);

    // Count frames as dropped if and only if we updated the frame but didn't
    // finish notifying the compositor nor managed to composite the current
    // frame.
    if (!current_frame_composited_ && !compositor_notify_finished_ &&
        frames_dropped_before_composite_ < kuint32max) {
      ++frames_dropped_before_composite_;
    }

    if (current_frame_ &&
        current_frame_->natural_size() != frame->natural_size()) {
      natural_size_changed_cb_.Run(frame->natural_size());
    }

    current_frame_ = frame;
    current_frame_composited_ = false;

    compositor_notify_finished_ = false;
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&Internal::NotifyCompositorOfNewFrame,
                   base::Unretained(this)));
  }

  // If |frame_being_composited| is true the current frame will not be counted
  // as being dropped the next time UpdateCurrentFrame() is called.
  scoped_refptr<media::VideoFrame> GetCurrentFrame(
      bool frame_being_composited) {
    base::AutoLock auto_lock(lock_);
    if (frame_being_composited)
      current_frame_composited_ = false;
    return current_frame_;
  }

  uint32 GetFramesDroppedBeforeComposite() {
    base::AutoLock auto_lock(lock_);
    return frames_dropped_before_composite_;
  }

  void SetFramesDroppedBeforeCompositeForTesting(uint32 dropped_frames) {
    base::AutoLock auto_lock(lock_);
    frames_dropped_before_composite_ = dropped_frames;
  }

 private:
  void NotifyCompositorOfNewFrame() {
    base::AutoLock auto_lock(lock_);
    compositor_notify_finished_ = true;
    if (client_)
      client_->DidReceiveFrame();
  }

  // cc::VideoFrameProvider implementation.
  virtual void SetVideoFrameProviderClient(
      cc::VideoFrameProvider::Client* client) OVERRIDE {
    if (client_)
      client_->StopUsingProvider();
    client_ = client;
  }

  virtual scoped_refptr<media::VideoFrame> GetCurrentFrame() OVERRIDE {
    return GetCurrentFrame(true);
  }

  virtual void PutCurrentFrame(const scoped_refptr<media::VideoFrame>& frame)
      OVERRIDE {}

  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  base::Callback<void(gfx::Size)>natural_size_changed_cb_;

  cc::VideoFrameProvider::Client* client_;

  base::Lock lock_;
  scoped_refptr<media::VideoFrame> current_frame_;
  bool compositor_notify_finished_;
  bool current_frame_composited_;
  uint32 frames_dropped_before_composite_;

  DISALLOW_COPY_AND_ASSIGN(Internal);
};

VideoFrameCompositor::VideoFrameCompositor(
    const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner,
    const base::Callback<void(gfx::Size)>& natural_size_changed_cb)
    : internal_(new Internal(compositor_task_runner, natural_size_changed_cb)) {
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
  return internal_->GetCurrentFrame(false);
}

uint32 VideoFrameCompositor::GetFramesDroppedBeforeComposite() {
  return internal_->GetFramesDroppedBeforeComposite();
}

void VideoFrameCompositor::SetFramesDroppedBeforeCompositeForTesting(
    uint32 dropped_frames) {
  internal_->SetFramesDroppedBeforeCompositeForTesting(dropped_frames);
}

}  // namespace content
