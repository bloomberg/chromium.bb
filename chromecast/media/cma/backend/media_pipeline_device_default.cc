// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/media_pipeline_device_default.h"

#include "chromecast/media/cma/backend/audio_pipeline_device_default.h"
#include "chromecast/media/cma/backend/media_clock_device_default.h"
#include "chromecast/media/cma/backend/video_pipeline_device_default.h"

namespace chromecast {
namespace media {

MediaPipelineDeviceDefault::MediaPipelineDeviceDefault()
    : media_clock_device_(new MediaClockDeviceDefault()),
      audio_pipeline_device_(
          new AudioPipelineDeviceDefault(media_clock_device_.get())),
      video_pipeline_device_(
          new VideoPipelineDeviceDefault(media_clock_device_.get())) {
}

MediaPipelineDeviceDefault::~MediaPipelineDeviceDefault() {
}

AudioPipelineDevice* MediaPipelineDeviceDefault::GetAudioPipelineDevice()
    const {
  return audio_pipeline_device_.get();
}

VideoPipelineDevice* MediaPipelineDeviceDefault::GetVideoPipelineDevice()
    const {
  return video_pipeline_device_.get();
}

MediaClockDevice* MediaPipelineDeviceDefault::GetMediaClockDevice() const {
  return media_clock_device_.get();
}

}  // namespace media
}  // namespace chromecast
