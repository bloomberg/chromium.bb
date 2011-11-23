// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains commonly used definitions of video capture.

#ifndef CONTENT_COMMON_MEDIA_VIDEO_CAPTURE_H_
#define CONTENT_COMMON_MEDIA_VIDEO_CAPTURE_H_

namespace video_capture {

// Current status of the video capture device. It's used by multiple classes
// in browser process and renderer process.
// Browser process sends information about the current capture state and
// error to the renderer process using this type.
enum State {
  kStarting,
  kStarted,
  kPaused,
  kStopping,
  kStopped,
  kError,
};

}  // namespace video_capture

#endif  // CONTENT_COMMON_MEDIA_VIDEO_CAPTURE_H_
