// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/alsa/slew_volume.h"

#include <algorithm>

#include "base/logging.h"
#include "media/base/vector_math.h"

namespace {

// The time to slew from current volume to target volume.
const int kMaxSlewTimeMs = 100;
const int kDefaultSampleRate = 44100;
}

namespace chromecast {
namespace media {

SlewVolume::SlewVolume() : SlewVolume(kMaxSlewTimeMs, kMaxSlewTimeMs) {}

SlewVolume::SlewVolume(int max_slew_time_up_ms, int max_slew_time_down_ms)
    : sample_rate_(kDefaultSampleRate),
      max_slew_time_up_ms_(max_slew_time_up_ms),
      max_slew_time_down_ms_(max_slew_time_down_ms),
      max_slew_up_(1000.0 / (max_slew_time_up_ms * sample_rate_)),
      max_slew_down_(1000.0 / (max_slew_time_down_ms * sample_rate_)) {
  LOG(INFO) << "Creating a slew volume: " << max_slew_time_up_ms;
}

void SlewVolume::SetSampleRate(int sample_rate) {
  sample_rate_ = sample_rate;
  SetVolume(volume_scale_);
}

// Slew rate should be volume_to_slew / slew_time / sample_rate
void SlewVolume::SetVolume(double volume_scale) {
  volume_scale_ = volume_scale;
  if (volume_scale_ > current_volume_) {
    max_slew_up_ = (volume_scale_ - current_volume_) * 1000.0 /
                   (max_slew_time_up_ms_ * sample_rate_);
  } else {
    max_slew_down_ = (current_volume_ - volume_scale_) * 1000.0 /
                     (max_slew_time_down_ms_ * sample_rate_);
  }
}

void SlewVolume::ProcessFMAC(bool repeat_transition,
                             const float* src,
                             int frames,
                             float* dest) {
  DCHECK(src);
  DCHECK(dest);
  // Ensure |src| and |dest| are 16-byte aligned.
  DCHECK_EQ(0u, reinterpret_cast<uintptr_t>(src) &
                    (::media::vector_math::kRequiredAlignment - 1));
  DCHECK_EQ(0u, reinterpret_cast<uintptr_t>(dest) &
                    (::media::vector_math::kRequiredAlignment - 1));

  if (!frames) {
    return;
  }

  if (repeat_transition) {
    current_volume_ = last_starting_volume_;
  } else {
    last_starting_volume_ = current_volume_;
  }

  if (current_volume_ == volume_scale_) {
    if (current_volume_ == 0.0) {
      return;
    }
    ::media::vector_math::FMAC(src, current_volume_, frames, dest);
    return;
  } else if (current_volume_ < volume_scale_) {
    do {
      (*dest) += (*src) * current_volume_;
      ++src;
      ++dest;
      --frames;
      current_volume_ += max_slew_up_;
    } while (current_volume_ < volume_scale_ && frames);
    current_volume_ = std::min(current_volume_, volume_scale_);
  } else {  // current_volume_ > volume_scale_
    do {
      (*dest) += (*src) * current_volume_;
      ++src;
      ++dest;
      --frames;
      current_volume_ -= max_slew_down_;
    } while (current_volume_ > volume_scale_ && frames);
    current_volume_ = std::max(current_volume_, volume_scale_);
  }

  if (frames) {
    for (int f = 0; f < frames; ++f) {
      dest[f] += src[f] * current_volume_;
    }
  }
}

// Scaling samples naively like this takes 0.2% of the CPU's time @ 44100hz
// on pineapple.
// Assumes 2 channel audio.
bool SlewVolume::ProcessInterleaved(int32_t* data, int frames) {
  DCHECK(data);

  if (!frames) {
    return true;
  }

  if (current_volume_ == volume_scale_) {
    if (current_volume_ == 1.0) {
      return true;
    }
    for (int i = 0; i < 2 * frames; ++i) {
      data[i] *= current_volume_;
    }
    return true;
  } else if (current_volume_ < volume_scale_) {
    do {
      (*data) *= current_volume_;
      ++data;
      (*data) *= current_volume_;
      ++data;
      --frames;
      current_volume_ += max_slew_up_;
    } while (current_volume_ < volume_scale_ && frames);
    current_volume_ = std::min(current_volume_, volume_scale_);
  } else {
    do {
      (*data) *= current_volume_;
      ++data;
      (*data) *= current_volume_;
      ++data;
      --frames;
      current_volume_ -= max_slew_down_;
    } while (current_volume_ > volume_scale_ && frames);
    current_volume_ = std::max(current_volume_, volume_scale_);
  }

  if (current_volume_ == 1.0) {
    return true;
  }

  if (current_volume_ == 0.0) {
    std::fill_n(data, frames * 2, 0);
    return true;
  }

  for (int i = 0; i < 2 * frames; ++i) {
    data[i] *= current_volume_;
  }
  return true;
}

}  // namespace media
}  // namespace chromecast
