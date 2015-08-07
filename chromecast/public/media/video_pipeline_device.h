// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_MEDIA_VIDEO_PIPELINE_DEVICE_H_
#define CHROMECAST_PUBLIC_MEDIA_VIDEO_PIPELINE_DEVICE_H_

#include "media_component_device.h"

namespace chromecast {
struct Size;

namespace media {
struct VideoConfig;

// Interface for platform-specific video pipeline backend.
// See comments on MediaComponentDevice.
//
// Notes:
// - Like a regular MediaComponentDevice, frames are possibly rendered only
//   in the kRunning state.
//   However, the first frame must be rendered regardless of the clock state:
//   - no synchronization needed to display the first frame,
//   - the clock rate has no impact on the presentation of the first frame.
class VideoPipelineDevice : public MediaComponentDevice {
 public:
  // Callback interface for natural size of video changing.
  class VideoClient {
   public:
    virtual ~VideoClient() {}
    virtual void OnNaturalSizeChanged(const Size& size) = 0;
  };

  ~VideoPipelineDevice() override {}

  // Registers |client| as the video specific event handler.
  // Implementation takes ownership of |client|.
  virtual void SetVideoClient(VideoClient* client) = 0;

  // Provides the video configuration.
  // Called before switching from |kStateUninitialized| to |kStateIdle|.
  // Afterwards, this can be invoked any time the configuration changes.
  // Returns true if the configuration is a supported configuration.
  virtual bool SetConfig(const VideoConfig& config) = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_PIPELINE_DEVICE_H_
