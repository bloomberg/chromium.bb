// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_DEVICE_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_DEVICE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace chromecast {
namespace media {
class AudioPipelineDevice;
class MediaClockDevice;
class MediaPipelineDeviceParams;
class VideoPipelineDevice;

// MediaPipelineDevice is the owner of the underlying audio/video/clock
// devices.
class MediaPipelineDevice {
 public:
  MediaPipelineDevice();
  virtual ~MediaPipelineDevice();

  virtual AudioPipelineDevice* GetAudioPipelineDevice() const = 0;

  virtual VideoPipelineDevice* GetVideoPipelineDevice() const = 0;

  virtual MediaClockDevice* GetMediaClockDevice() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaPipelineDevice);
};

// Factory method to create a MediaPipelineDevice.
typedef base::Callback<scoped_ptr<MediaPipelineDevice>(
    const MediaPipelineDeviceParams&)> CreatePipelineDeviceCB;

// Direct creation of vendor-specific media device pipeline.
// TODO(halliwell): move into libcast_media
scoped_ptr<MediaPipelineDevice> CreateMediaPipelineDevice(
    const MediaPipelineDeviceParams& params);

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_DEVICE_H_
