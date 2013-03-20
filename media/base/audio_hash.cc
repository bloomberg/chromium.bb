// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MSVC++ requires this to be set before any other includes to get M_PI.
#define _USE_MATH_DEFINES
#include <cmath>

#include "media/base/audio_hash.h"

#include "base/stringprintf.h"
#include "media/base/audio_bus.h"

namespace media {

AudioHash::AudioHash()
    : audio_hash_(),
      hash_count_(0) {
  COMPILE_ASSERT(arraysize(audio_hash_) == kHashBuckets, audio_hash_size_error);
}

AudioHash::~AudioHash() {}

void AudioHash::Update(const AudioBus* audio_bus, int frames) {
  for (int ch = 0; ch < audio_bus->channels(); ++ch) {
    const float* channel = audio_bus->channel(ch);
    for (int i = 0; i < frames; ++i) {
      const int kHashIndex =
          (i * (ch + 1) + hash_count_) % arraysize(audio_hash_);

      // Mix in a sine wave with the result so we ensure that sequences of empty
      // buffers don't result in an empty hash.
      if (ch == 0) {
        audio_hash_[kHashIndex] += channel[i] + sin(2.0 * M_PI * M_PI * i);
      } else {
        audio_hash_[kHashIndex] += channel[i];
      }
    }
  }

  ++hash_count_;
}

std::string AudioHash::ToString() const {
  std::string result;
  for (size_t i = 0; i < arraysize(audio_hash_); ++i)
    result += base::StringPrintf("%.2f,", audio_hash_[i]);
  return result;
}

}  // namespace media