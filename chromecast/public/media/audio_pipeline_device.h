// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_MEDIA_AUDIO_PIPELINE_DEVICE_H_
#define CHROMECAST_PUBLIC_MEDIA_AUDIO_PIPELINE_DEVICE_H_

#include "media_component_device.h"

namespace chromecast {
namespace media {
class AudioPipelineDeviceClient;
struct AudioConfig;

// Interface for platform-specific audio pipeline backend.
// See comments on MediaComponentDevice.
class AudioPipelineDevice : public MediaComponentDevice {
 public:
  ~AudioPipelineDevice() override {}

  // Provides the audio configuration.
  // Will be called before switching from |kStateUninitialized| to |kStateIdle|.
  // Afterwards, this can be invoked any time the configuration changes.
  // Returns true if the configuration is a supported configuration.
  virtual bool SetConfig(const AudioConfig& config) = 0;

  // Sets the volume multiplier.
  // The multiplier is in the range [0.0, 1.0].
  virtual void SetStreamVolumeMultiplier(float multiplier) = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_PIPELINE_DEVICE_H_
