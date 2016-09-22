// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_BUFFER_TRACKER_FACTORY_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_BUFFER_TRACKER_FACTORY_H_

#include <memory>

#include "content/browser/renderer_host/media/video_capture_buffer_tracker.h"

namespace content {

class VideoCaptureBufferTrackerFactory {
 public:
  static std::unique_ptr<VideoCaptureBufferTracker> CreateTracker(
      media::VideoPixelStorage storage);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_BUFFER_TRACKER_FACTORY_H_
