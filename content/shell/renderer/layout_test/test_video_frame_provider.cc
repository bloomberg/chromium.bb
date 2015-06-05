// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/layout_test/test_video_frame_provider.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "media/base/video_frame.h"

namespace content {

TestVideoFrameProvider::TestVideoFrameProvider(
    const gfx::Size& size,
    const base::TimeDelta& frame_duration,
    const base::Closure& error_cb,
    const VideoFrameProvider::RepaintCB& repaint_cb)
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      size_(size),
      state_(kStopped),
      frame_duration_(frame_duration),
      error_cb_(error_cb),
      repaint_cb_(repaint_cb) {
}

TestVideoFrameProvider::~TestVideoFrameProvider() {}

void TestVideoFrameProvider::Start() {
  DVLOG(1) << "TestVideoFrameProvider::Start";
  DCHECK(task_runner_->BelongsToCurrentThread());
  state_ = kStarted;
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&TestVideoFrameProvider::GenerateFrame, this));
}

void TestVideoFrameProvider::Stop() {
  DVLOG(1) << "TestVideoFrameProvider::Stop";
  DCHECK(task_runner_->BelongsToCurrentThread());
  state_ = kStopped;
}

void TestVideoFrameProvider::Play() {
  DVLOG(1) << "TestVideoFrameProvider::Play";
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (state_ == kPaused)
    state_ = kStarted;
}

void TestVideoFrameProvider::Pause() {
  DVLOG(1) << "TestVideoFrameProvider::Pause";
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (state_ == kStarted)
    state_ = kPaused;
}

void TestVideoFrameProvider::GenerateFrame() {
  DVLOG(1) << "TestVideoFrameProvider::GenerateFrame";
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (state_ == kStopped)
    return;

  if (state_ == kStarted) {
    // Always allocate a new frame filled with white color.
    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::CreateColorFrame(
            size_, 255, 128, 128, current_time_);

    // TODO(wjia): set pixel data to pre-defined patterns if it's desired to
    // verify frame content.

    repaint_cb_.Run(video_frame);
  }

  current_time_ += frame_duration_;
  task_runner_->PostDelayedTask(
      FROM_HERE, base::Bind(&TestVideoFrameProvider::GenerateFrame, this),
      frame_duration_);
}

}  // namespace content
