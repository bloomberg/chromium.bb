// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/utility/audio_utility.h"

#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/cast/cast_config.h"

namespace media {
namespace cast {

TestAudioBusFactory::TestAudioBusFactory(int num_channels,
                                         int sample_rate,
                                         float sine_wave_frequency,
                                         float volume)
    : num_channels_(num_channels),
      sample_rate_(sample_rate),
      volume_(volume),
      source_(num_channels, sine_wave_frequency, sample_rate) {
  CHECK_LT(0, num_channels);
  CHECK_LT(0, sample_rate);
  CHECK_LE(0.0f, volume_);
  CHECK_LE(volume_, 1.0f);
}

TestAudioBusFactory::~TestAudioBusFactory() {}

scoped_ptr<AudioBus> TestAudioBusFactory::NextAudioBus(
    const base::TimeDelta& duration) {
  const int num_samples = static_cast<int>((sample_rate_ * duration) /
                                           base::TimeDelta::FromSeconds(1));
  scoped_ptr<AudioBus> bus(AudioBus::Create(num_channels_, num_samples));
  source_.OnMoreData(bus.get(), AudioBuffersState());
  bus->Scale(volume_);
  return bus.Pass();
}

scoped_ptr<PcmAudioFrame> ToPcmAudioFrame(const AudioBus& audio_bus,
                                          int sample_rate) {
  scoped_ptr<PcmAudioFrame> audio_frame(new PcmAudioFrame());
  audio_frame->channels = audio_bus.channels();
  audio_frame->frequency = sample_rate;
  audio_frame->samples.resize(audio_bus.channels() * audio_bus.frames());
  audio_bus.ToInterleaved(audio_bus.frames(),
                          sizeof(audio_frame->samples.front()),
                          &audio_frame->samples.front());
  return audio_frame.Pass();
}

int CountZeroCrossings(const std::vector<int16>& samples) {
  // The sample values must pass beyond |kAmplitudeThreshold| on the opposite
  // side of zero before a crossing will be counted.
  const int kAmplitudeThreshold = 1000;  // Approx. 3% of max amplitude.

  int count = 0;
  std::vector<int16>::const_iterator i = samples.begin();
  int16 last = 0;
  for (; i != samples.end() && abs(last) < kAmplitudeThreshold; ++i)
    last = *i;
  for (; i != samples.end(); ++i) {
    if (abs(*i) >= kAmplitudeThreshold && (last < 0) != (*i < 0)) {
      ++count;
      last = *i;
    }
  }
  return count;
}

}  // namespace cast
}  // namespace media
