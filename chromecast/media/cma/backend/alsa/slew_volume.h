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
  SlewVolume(int max_slew_time_ms);
  ~SlewVolume() = default;

  void SetSampleRate(int sample_rate);
  void SetVolume(double volume_scale);
  void SetMaxSlewTimeMs(int max_slew_time_ms);

  // Called to indicate that the stream was interrupted; volume changes can be
  // applied immediately.
  void Interrupted();

  // Assumes 1 channel float data that is 16-byte aligned. Smoothly calculates
  // dest[i] += src[i] * volume_scaling
  // ProcessFMAC will be called once for each channel of audio present and
  // |repeat_transition| will be true for channels 2 through n.
  void ProcessFMAC(bool repeat_transition,
                   const float* src,
                   int frames,
                   float* dest);

  // Processes a single channel of float data
  // dest[i] = src[i] * volume_scaling.
  // ProcessFMUL will be called once for each channel of audio present.
  // |repeat_transition| should be true for channels 2 through n.
  // Assumes 2 channels.
  void ProcessFMUL(bool repeat_transition,
                   const float* src,
                   int frames,
                   float* dest);

 private:
  template <typename Traits>
  void ProcessData(bool repeat_transition,
                   const float* src,
                   int frames,
                   float* dest);

  double sample_rate_;
  double volume_scale_ = 1.0;
  double current_volume_ = 1.0;
  double last_starting_volume_ = 1.0;
  double max_slew_time_ms_;
  double max_slew_per_sample_;
  bool interrupted_ = true;

  DISALLOW_COPY_AND_ASSIGN(SlewVolume);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ALSA_SLEW_VOLUME_H_
