// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/media_pipeline_device.h"

#include "chromecast/media/cma/backend/audio_pipeline_device_default.h"
#include "chromecast/media/cma/backend/media_clock_device_default.h"
#include "chromecast/media/cma/backend/media_pipeline_device_factory.h"
#include "chromecast/media/cma/backend/video_pipeline_device_default.h"

namespace chromecast {
namespace media {

MediaPipelineDevice::MediaPipelineDevice(
    scoped_ptr<MediaPipelineDeviceFactory> factory)
    : MediaPipelineDevice(factory->CreateMediaClockDevice(),
                          factory->CreateAudioPipelineDevice(),
                          factory->CreateVideoPipelineDevice()) {
}

MediaPipelineDevice::MediaPipelineDevice(
    scoped_ptr<MediaClockDevice> media_clock_device,
    scoped_ptr<AudioPipelineDevice> audio_pipeline_device,
    scoped_ptr<VideoPipelineDevice> video_pipeline_device)
    : media_clock_device_(media_clock_device.Pass()),
      audio_pipeline_device_(audio_pipeline_device.Pass()),
      video_pipeline_device_(video_pipeline_device.Pass()) {
}

MediaPipelineDevice::~MediaPipelineDevice() {
}

}  // namespace media
}  // namespace chromecast
