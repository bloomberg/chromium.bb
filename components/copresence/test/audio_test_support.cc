// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/test/audio_test_support.h"

#include <cstdlib>

#include "media/base/audio_bus.h"

namespace copresence {

void PopulateSamples(int random_seed, size_t size, float* samples) {
  srand(random_seed);
  for (size_t i = 0; i < size; ++i)
    samples[i] = (2.0 * rand() / RAND_MAX) - 1;
}

scoped_ptr<media::AudioBus> CreateRandomAudio(int random_seed,
                                              int channels,
                                              int samples) {
  scoped_ptr<media::AudioBus> bus = media::AudioBus::Create(channels, samples);
  for (int ch = 0; ch < channels; ++ch)
    PopulateSamples(random_seed, samples, bus->channel(ch));
  return bus.Pass();
}

scoped_refptr<media::AudioBusRefCounted>
CreateRandomAudioRefCounted(int random_seed, int channels, int samples) {
  scoped_refptr<media::AudioBusRefCounted> bus =
      media::AudioBusRefCounted::Create(channels, samples);
  for (int ch = 0; ch < channels; ++ch)
    PopulateSamples(random_seed, samples, bus->channel(ch));
  return bus;
}

}  // namespace copresence
