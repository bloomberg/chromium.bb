// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/audio_modem/test/random_samples.h"

#include <cstdlib>

#include "media/base/audio_bus.h"

namespace audio_modem {

void PopulateSamples(unsigned int random_seed, size_t size, float* samples) {
// On Windows, rand() is threadsafe, and rand_r() is not available.
#if defined(OS_WIN)
  srand(random_seed);
  for (size_t i = 0; i < size; ++i)
    samples[i] = (2.0 * rand() / RAND_MAX) - 1;  // NOLINT
#else
  for (size_t i = 0; i < size; ++i)
    samples[i] = (2.0 * rand_r(&random_seed) / RAND_MAX) - 1;
#endif
}

scoped_refptr<media::AudioBusRefCounted>
CreateRandomAudioRefCounted(int random_seed, int channels, int samples) {
  scoped_refptr<media::AudioBusRefCounted> bus =
      media::AudioBusRefCounted::Create(channels, samples);
  for (int ch = 0; ch < channels; ++ch)
    PopulateSamples(random_seed, samples, bus->channel(ch));
  return bus;
}

}  // namespace audio_modem
