// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_DEVICE_FAKE_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_DEVICE_FAKE_H_

#include "chromecast/media/cma/backend/media_pipeline_device.h"

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace chromecast {
namespace media {
class AudioPipelineDeviceFake;
class MediaClockDeviceFake;
class VideoPipelineDeviceFake;

class MediaPipelineDeviceFake : public MediaPipelineDevice {
 public:
  MediaPipelineDeviceFake();
  ~MediaPipelineDeviceFake() override;

  // MediaPipelineDevice implementation.
  AudioPipelineDevice* GetAudioPipelineDevice() const override;
  VideoPipelineDevice* GetVideoPipelineDevice() const override;
  MediaClockDevice* GetMediaClockDevice() const override;

 private:
  scoped_ptr<MediaClockDeviceFake> media_clock_device_;
  scoped_ptr<AudioPipelineDeviceFake> audio_pipeline_device_;
  scoped_ptr<VideoPipelineDeviceFake> video_pipeline_device_;

  DISALLOW_COPY_AND_ASSIGN(MediaPipelineDeviceFake);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_DEVICE_H_
