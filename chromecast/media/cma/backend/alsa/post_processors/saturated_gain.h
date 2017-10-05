// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ALSA_POST_PROCESSORS_SATURATED_GAIN_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ALSA_POST_PROCESSORS_SATURATED_GAIN_H_

#include <string>

#include "base/macros.h"
#include "chromecast/media/cma/backend/alsa/slew_volume.h"
#include "chromecast/public/media/audio_post_processor_shlib.h"

namespace chromecast {
namespace media {

// Provides a simple gain, avoiding overflow.
class SaturatedGain : public AudioPostProcessor {
 public:
  SaturatedGain(const std::string& config, int channels);
  ~SaturatedGain() override;

  // AudioPostProcessor implementation:
  bool SetSampleRate(int sample_rate) override;
  int ProcessFrames(float* data,
                    int frames,
                    float volume,
                    float volume_dbfs) override;
  int GetRingingTimeInFrames() override;

 private:
  int channels_;
  int sample_rate_;
  float last_volume_dbfs_;
  SlewVolume slew_volume_;
  float gain_;

  DISALLOW_COPY_AND_ASSIGN(SaturatedGain);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ALSA_POST_PROCESSORS_SATURATED_GAIN_H_
