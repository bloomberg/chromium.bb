// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUDIO_MODEM_TEST_RANDOM_SAMPLES_H_
#define COMPONENTS_AUDIO_MODEM_TEST_RANDOM_SAMPLES_H_

#include <stddef.h>

#include "base/memory/ref_counted.h"

namespace media {
class AudioBus;
class AudioBusRefCounted;
}

namespace audio_modem {

// Populate random samples given a random seed into the samples array.
void PopulateSamples(unsigned int random_seed, size_t size, float* samples);

// Create an ref counted audio bus populated with random samples.
scoped_refptr<media::AudioBusRefCounted>
    CreateRandomAudioRefCounted(int random_seed, int channels, int samples);

}  // namespace audio_modem

#endif  // COMPONENTS_AUDIO_MODEM_TEST_RANDOM_SAMPLES_H_
