// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_audio_level_calculator.h"

#include <cmath>

#include "base/logging.h"
#include "base/stl_util.h"
#include "media/base/audio_bus.h"

namespace content {

namespace {

// Calculates the maximum absolute amplitude of the audio data.
float MaxAmplitude(const float* audio_data, int length) {
  float max = 0.0f;
  for (int i = 0; i < length; ++i) {
    const float absolute = fabsf(audio_data[i]);
    if (absolute > max)
      max = absolute;
  }
  DCHECK(std::isfinite(max));
  return max;
}

}  // namespace

MediaStreamAudioLevelCalculator::MediaStreamAudioLevelCalculator()
    : counter_(0),
      max_amplitude_(0.0f),
      level_(0.0f) {
}

MediaStreamAudioLevelCalculator::~MediaStreamAudioLevelCalculator() {
}

float MediaStreamAudioLevelCalculator::Calculate(
    const media::AudioBus& audio_bus) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // |level_| is updated every 10 callbacks. For the case where callback comes
  // every 10ms, |level_| will be updated approximately every 100ms.
  static const int kUpdateFrequency = 10;

  float max = 0.0f;
  for (int i = 0; i < audio_bus.channels(); ++i) {
    const float max_this_channel =
        MaxAmplitude(audio_bus.channel(i), audio_bus.frames());
    if (max_this_channel > max)
      max = max_this_channel;
  }
  max_amplitude_ = std::max(max_amplitude_, max);

  if (counter_++ == kUpdateFrequency) {
    level_ = max_amplitude_;

    // Decay the absolute maximum amplitude by 1/4.
    max_amplitude_ /= 4.0f;

    // Reset the counter.
    counter_ = 0;
  }

  return level_;
}

}  // namespace content
