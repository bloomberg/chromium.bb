// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/media_pipeline_backend_default.h"

#include "chromecast/media/cma/backend/audio_pipeline_device_default.h"
#include "chromecast/media/cma/backend/media_clock_device_default.h"
#include "chromecast/media/cma/backend/video_pipeline_device_default.h"

namespace chromecast {
namespace media {

MediaPipelineBackendDefault::MediaPipelineBackendDefault(
    const MediaPipelineDeviceParams& params)
    : params_(params) {}

MediaPipelineBackendDefault::~MediaPipelineBackendDefault() {}

MediaClockDevice* MediaPipelineBackendDefault::GetClock() {
  if (!clock_)
    clock_.reset(new MediaClockDeviceDefault());
  return clock_.get();
}

AudioPipelineDevice* MediaPipelineBackendDefault::GetAudio() {
  if (!audio_)
    audio_.reset(new AudioPipelineDeviceDefault(params_, GetClock()));
  return audio_.get();
}

VideoPipelineDevice* MediaPipelineBackendDefault::GetVideo() {
  if (!video_)
    video_.reset(new VideoPipelineDeviceDefault(params_, GetClock()));
  return video_.get();
}

}  // namespace media
}  // namespace chromecast
