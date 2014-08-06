// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_COMMON_AUDIO_TEST_SUPPORT_
#define COMPONENTS_COPRESENCE_COMMON_AUDIO_TEST_SUPPORT_

#include <cstddef>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace media {
class AudioBus;
class AudioBusRefCounted;
}

namespace copresence {

// Populate random samples given a random seed into the samples array.
void PopulateSamples(int random_seed, size_t size, float* samples);

// Create an audio bus populated with random samples.
scoped_ptr<media::AudioBus> CreateRandomAudio(int random_seed,
                                              int channels,
                                              int samples);

// Create an ref counted audio bus populated with random samples.
scoped_refptr<media::AudioBusRefCounted>
    CreateRandomAudioRefCounted(int random_seed, int channels, int samples);

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_COMMON_AUDIO_TEST_SUPPORT_
