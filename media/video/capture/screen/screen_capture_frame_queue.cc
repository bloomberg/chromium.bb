// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/screen/screen_capture_frame_queue.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/threading/non_thread_safe.h"
#include "media/video/capture/screen/shared_desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace media {

ScreenCaptureFrameQueue::ScreenCaptureFrameQueue() : current_(0) {}

ScreenCaptureFrameQueue::~ScreenCaptureFrameQueue() {}

void ScreenCaptureFrameQueue::MoveToNextFrame() {
  current_ = (current_ + 1) % kQueueLength;

  // Verify that the frame is not shared, i.e. that consumer has released it
  // before attempting to capture again.
  DCHECK(!frames_[current_] || !frames_[current_]->IsShared());
}

void ScreenCaptureFrameQueue::ReplaceCurrentFrame(
    scoped_ptr<webrtc::DesktopFrame> frame) {
  frames_[current_].reset(SharedDesktopFrame::Wrap(frame.release()));
}

void ScreenCaptureFrameQueue::Reset() {
  for (int i = 0; i < kQueueLength; ++i)
    frames_[i].reset();
}

}  // namespace media
