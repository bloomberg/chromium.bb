// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_PIPELINE_DEVICE_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_PIPELINE_DEVICE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "chromecast/media/cma/backend/media_component_device.h"

namespace gfx {
class Size;
}

namespace media {
class VideoDecoderConfig;
}

namespace chromecast {
namespace media {
class DecoderBufferBase;

// VideoPipelineDevice -
//
// Notes:
// - Like a regular MediaComponentDevice, frames are possibly rendered only
//   in the kRunning state.
//   However, the first frame must be rendered regardless of the clock state:
//   - no synchronization needed to display the first frame,
//   - the clock rate has no impact on the presentation of the first frame.
//
class VideoPipelineDevice : public MediaComponentDevice {
 public:
  struct VideoClient {
    VideoClient();
    ~VideoClient();

    // Invoked each time the natural size is updated.
    base::Callback<void(const gfx::Size& natural_size)>
        natural_size_changed_cb;
  };

  VideoPipelineDevice();
  ~VideoPipelineDevice() override;

  // Registers |client| as the video specific event handler.
  virtual void SetVideoClient(const VideoClient& client) = 0;

  // Provide the video configuration.
  // Must be called before switching from |kStateUninitialized| to |kStateIdle|.
  // Afterwards, this can be invoked any time the configuration changes.
  // Returns true if the configuration is a supported configuration.
  virtual bool SetConfig(const ::media::VideoDecoderConfig& config) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoPipelineDevice);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_PIPELINE_DEVICE_H_
