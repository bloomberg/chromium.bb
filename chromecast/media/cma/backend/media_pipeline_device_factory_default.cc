// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/media_pipeline_device_factory_default.h"

#include "chromecast/media/cma/backend/audio_pipeline_device_default.h"
#include "chromecast/media/cma/backend/media_clock_device_default.h"
#include "chromecast/media/cma/backend/video_pipeline_device_default.h"

namespace chromecast {
namespace media {

MediaPipelineDeviceFactoryDefault::MediaPipelineDeviceFactoryDefault()
    : clock_(nullptr) {
}

scoped_ptr<MediaClockDevice>
MediaPipelineDeviceFactoryDefault::CreateMediaClockDevice() {
  DCHECK(!clock_);
  clock_ = new MediaClockDeviceDefault();
  return make_scoped_ptr(clock_);
}

scoped_ptr<AudioPipelineDevice>
MediaPipelineDeviceFactoryDefault::CreateAudioPipelineDevice() {
  DCHECK(clock_);
  return make_scoped_ptr(new AudioPipelineDeviceDefault(clock_));
}

scoped_ptr<VideoPipelineDevice>
MediaPipelineDeviceFactoryDefault::CreateVideoPipelineDevice() {
  DCHECK(clock_);
  return make_scoped_ptr(new VideoPipelineDeviceDefault(clock_));
}

}  // namespace media
}  // namespace chromecast
