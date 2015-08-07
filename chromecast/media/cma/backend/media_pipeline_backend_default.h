// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_DEVICE_FACTORY_DEFAULT_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_DEVICE_FACTORY_DEFAULT_H_

#include "base/memory/scoped_ptr.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/media/media_pipeline_device_params.h"

namespace chromecast {
namespace media {

// Factory that instantiates default (stub) media pipeline device elements.
class MediaPipelineBackendDefault : public MediaPipelineBackend {
 public:
  MediaPipelineBackendDefault(const MediaPipelineDeviceParams& params);
  ~MediaPipelineBackendDefault() override;

  // MediaPipelineBackend implementation
  MediaClockDevice* GetClock() override;
  AudioPipelineDevice* GetAudio() override;
  VideoPipelineDevice* GetVideo() override;

 private:
  MediaPipelineDeviceParams params_;
  scoped_ptr<MediaClockDevice> clock_;
  scoped_ptr<AudioPipelineDevice> audio_;
  scoped_ptr<VideoPipelineDevice> video_;

  DISALLOW_COPY_AND_ASSIGN(MediaPipelineBackendDefault);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_DEVICE_FACTORY_DEFAULT_H_
