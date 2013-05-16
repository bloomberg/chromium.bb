// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_SCREEN_SCREEN_CAPTURE_FRAME_QUEUE_H_
#define MEDIA_VIDEO_CAPTURE_SCREEN_SCREEN_CAPTURE_FRAME_QUEUE_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "media/video/capture/screen/shared_desktop_frame.h"

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

namespace media {

// Represents a queue of reusable video frames. Provides access to the 'current'
// frame - the frame that the caller is working with at the moment, and to the
// 'previous' frame - the predecessor of the current frame swapped by
// MoveToNextFrame() call, if any.
//
// The caller is expected to (re)allocate frames if current_frame() returns
// NULL. The caller can mark all frames in the queue for reallocation (when,
// say, frame dimensions change). The queue records which frames need updating
// which the caller can query.
//
// Frame consumer is expected to never hold more than kQueueLength frames
// created by this function and it should release the earliest one before trying
// to capture a new frame (i.e. before MoveToNextFrame() is called).
class ScreenCaptureFrameQueue {
 public:
  ScreenCaptureFrameQueue();
  ~ScreenCaptureFrameQueue();

  // Moves to the next frame in the queue, moving the 'current' frame to become
  // the 'previous' one.
  void MoveToNextFrame();

  // Replaces the current frame with a new one allocated by the caller.
  // The existing frame (if any) is destroyed.
  void ReplaceCurrentFrame(scoped_ptr<webrtc::DesktopFrame> frame);

  // Marks all frames obsolete and resets the previous frame pointer. No
  // frames are freed though as the caller can still access them.
  void Reset();

  SharedDesktopFrame* current_frame() const {
    return frames_[current_].get();
  }

  SharedDesktopFrame* previous_frame() const {
    return frames_[(current_ + kQueueLength - 1) % kQueueLength].get();
  }

 private:
  // Index of the current frame.
  int current_;

  static const int kQueueLength = 2;
  scoped_ptr<SharedDesktopFrame> frames_[kQueueLength];

  DISALLOW_COPY_AND_ASSIGN(ScreenCaptureFrameQueue);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_SCREEN_SCREEN_CAPTURE_FRAME_QUEUE_H_
