// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_PIPELINE_DEVICE_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_PIPELINE_DEVICE_H_

#include "base/macros.h"
#include "chromecast/media/cma/backend/media_component_device.h"

namespace media {
class AudioDecoderConfig;
}

namespace chromecast {
namespace media {
class AudioPipelineDeviceClient;

class AudioPipelineDevice : public MediaComponentDevice {
 public:
  AudioPipelineDevice();
  ~AudioPipelineDevice() override;

  // Provide the audio configuration.
  // Must be called before switching from |kStateUninitialized| to |kStateIdle|.
  // Afterwards, this can be invoked any time the configuration changes.
  // Returns true if the configuration is a supported configuration.
  virtual bool SetConfig(const ::media::AudioDecoderConfig& config) = 0;

  // Sets the volume multiplier.
  // The multiplier must be in the range [0.0, 1.0].
  virtual void SetStreamVolumeMultiplier(float multiplier) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioPipelineDevice);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_PIPELINE_DEVICE_H_
