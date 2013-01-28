// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/screen/screen_capture_frame_queue.h"

#include <algorithm>

#include "base/basictypes.h"
#include "media/video/capture/screen/screen_capture_frame.h"

namespace media {

ScreenCaptureFrameQueue::ScreenCaptureFrameQueue()
    : current_(0),
      previous_(NULL) {
  SetAllFramesNeedUpdate();
}

ScreenCaptureFrameQueue::~ScreenCaptureFrameQueue() {
}

void ScreenCaptureFrameQueue::DoneWithCurrentFrame() {
  previous_ = current_frame();
  current_ = (current_ + 1) % kQueueLength;
}

void ScreenCaptureFrameQueue::ReplaceCurrentFrame(
    scoped_ptr<ScreenCaptureFrame> frame) {
  frames_[current_] = frame.Pass();
  needs_update_[current_] = false;
}

void ScreenCaptureFrameQueue::SetAllFramesNeedUpdate() {
  std::fill(needs_update_, needs_update_ + arraysize(needs_update_), true);
  previous_ = NULL;
}

}  // namespace media
