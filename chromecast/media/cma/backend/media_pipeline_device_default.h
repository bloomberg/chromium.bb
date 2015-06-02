// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_DEVICE_DEFAULT_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_DEVICE_DEFAULT_H_

#include "chromecast/media/cma/backend/media_pipeline_device.h"

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace chromecast {
namespace media {
class AudioPipelineDeviceDefault;
class MediaClockDeviceDefault;
class VideoPipelineDeviceDefault;

class MediaPipelineDeviceDefault : public MediaPipelineDevice {
 public:
  MediaPipelineDeviceDefault();
  ~MediaPipelineDeviceDefault() override;

  // MediaPipelineDevice implementation.
  AudioPipelineDevice* GetAudioPipelineDevice() const override;
  VideoPipelineDevice* GetVideoPipelineDevice() const override;
  MediaClockDevice* GetMediaClockDevice() const override;

 private:
  scoped_ptr<MediaClockDeviceDefault> media_clock_device_;
  scoped_ptr<AudioPipelineDeviceDefault> audio_pipeline_device_;
  scoped_ptr<VideoPipelineDeviceDefault> video_pipeline_device_;

  DISALLOW_COPY_AND_ASSIGN(MediaPipelineDeviceDefault);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_DEVICE_DEFAULT_H_
