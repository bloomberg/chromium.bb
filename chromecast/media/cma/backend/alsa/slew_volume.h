// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Scale volume with slew rate limiting

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ALSA_SLEW_VOLUME_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ALSA_SLEW_VOLUME_H_

#include <stdint.h>

#include "base/macros.h"

namespace chromecast {
namespace media {

class SlewVolume {
 public:
  SlewVolume();
  SlewVolume(int max_slew_up_ms, int max_slew_down_ms);
  ~SlewVolume() = default;

  void SetSampleRate(int sample_rate);
  void SetVolume(double volume_scale);

  // Assumes 1 channel float data that is 16-byte aligned. Smoothly calculates
  // dest[i] += src[i] * volume_scaling
  // ProcessFMAC will be called once for each channel of audio present and
  // |repeat_transition| will be true for channels 2 through n.
  void ProcessFMAC(bool repeat_transition,
                   const float* src,
                   int frames,
                   float* dest);

  // Assumes 2 channels.
  bool ProcessInterleaved(int32_t* data, int frames);

 private:
  double sample_rate_;
  double volume_scale_ = 1.0;
  double current_volume_ = 1.0;
  double last_starting_volume_ = 1.0;
  int max_slew_time_up_ms_;
  int max_slew_time_down_ms_;
  double max_slew_up_;
  double max_slew_down_;

  DISALLOW_COPY_AND_ASSIGN(SlewVolume);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ALSA_SLEW_VOLUME_H_
