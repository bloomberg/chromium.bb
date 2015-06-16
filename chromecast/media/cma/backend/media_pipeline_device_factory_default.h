// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_DEVICE_FACTORY_DEFAULT_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_DEVICE_FACTORY_DEFAULT_H_

#include "chromecast/media/cma/backend/media_pipeline_device_factory.h"

namespace chromecast {
namespace media {

// Factory that instantiates default (stub) media pipeline device elements.
class MediaPipelineDeviceFactoryDefault : public MediaPipelineDeviceFactory {
 public:
  MediaPipelineDeviceFactoryDefault();
  ~MediaPipelineDeviceFactoryDefault() override {}

  // MediaPipelineDeviceFactory implementation
  scoped_ptr<MediaClockDevice> CreateMediaClockDevice() override;
  scoped_ptr<AudioPipelineDevice> CreateAudioPipelineDevice() override;
  scoped_ptr<VideoPipelineDevice> CreateVideoPipelineDevice() override;

 private:
  // Owned by MediaPipelineDevice, but we hold raw pointer in order to wire up
  // to audio and video devices.  MediaPipelineDevice guarantees the clock will
  // outlive the audio and video devices.
  MediaClockDevice* clock_;

  DISALLOW_COPY_AND_ASSIGN(MediaPipelineDeviceFactoryDefault);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_DEVICE_FACTORY_DEFAULT_H_
