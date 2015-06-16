// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_DEVICE_FACTORY_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_DEVICE_FACTORY_H_

#include "base/memory/scoped_ptr.h"

namespace chromecast {
namespace media {

class AudioPipelineDevice;
class MediaClockDevice;
class MediaPipelineDeviceParams;
class VideoPipelineDevice;

// Factory for creating platform-specific clock, audio and video devices.
// A new factory will be instantiated for each media player instance.
class MediaPipelineDeviceFactory {
 public:
  MediaPipelineDeviceFactory() {}
  virtual ~MediaPipelineDeviceFactory() {}

  virtual scoped_ptr<MediaClockDevice> CreateMediaClockDevice() = 0;
  virtual scoped_ptr<AudioPipelineDevice> CreateAudioPipelineDevice() = 0;
  virtual scoped_ptr<VideoPipelineDevice> CreateVideoPipelineDevice() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaPipelineDeviceFactory);
};

// Creates the platform-specific pipeline device factory.
// TODO(halliwell): move into libcast_media
scoped_ptr<MediaPipelineDeviceFactory> GetMediaPipelineDeviceFactory(
    const MediaPipelineDeviceParams& params);

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_DEVICE_FACTORY_H_
