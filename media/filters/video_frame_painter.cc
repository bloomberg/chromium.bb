// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/video_frame_painter.h"

#include "media/base/video_frame.h"

namespace media {

VideoFramePainter::VideoFramePainter(
    const base::Closure& invalidate_cb,
    const base::Callback<void(gfx::Size)>& natural_size_changed_cb)
    : invalidate_cb_(invalidate_cb),
      natural_size_changed_cb_(natural_size_changed_cb),
      invalidation_finished_(true),
      current_frame_painted_(false),
      frames_dropped_before_paint_(0) {}

VideoFramePainter::~VideoFramePainter() {}

void VideoFramePainter::UpdateCurrentFrame(
    const scoped_refptr<VideoFrame>& frame) {
  base::AutoLock auto_lock(lock_);

  // Count frames as dropped if and only if we updated the frame but didn't
  // finish invalidating nor managed to paint the current frame.
  if (!current_frame_painted_ && !invalidation_finished_ &&
      frames_dropped_before_paint_ < kuint32max) {
    ++frames_dropped_before_paint_;
  }

  if (current_frame_ &&
      current_frame_->natural_size() != frame->natural_size()) {
    natural_size_changed_cb_.Run(frame->natural_size());
  }

  current_frame_ = frame;
  current_frame_painted_ = false;

  if (!invalidation_finished_)
    return;

  invalidation_finished_ = false;
  invalidate_cb_.Run();
}

scoped_refptr<VideoFrame> VideoFramePainter::GetCurrentFrame(
    bool frame_being_painted) {
  base::AutoLock auto_lock(lock_);
  if (frame_being_painted)
    current_frame_painted_ = true;

  return current_frame_;
}

void VideoFramePainter::DidFinishInvalidating() {
  base::AutoLock auto_lock(lock_);
  invalidation_finished_ = true;
}

uint32 VideoFramePainter::GetFramesDroppedBeforePaint() {
  base::AutoLock auto_lock(lock_);
  return frames_dropped_before_paint_;
}

void VideoFramePainter::SetFramesDroppedBeforePaintForTesting(
    uint32 dropped_frames) {
  base::AutoLock auto_lock(lock_);
  frames_dropped_before_paint_ = dropped_frames;
}

}  // namespace media
