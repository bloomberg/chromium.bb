// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_common.h"

#include "base/time.h"
#include "media/audio/audio_parameters.h"

// The minimum number of samples in a hardware packet.
// This value is selected so that we can handle down to 5khz sample rate.
static const int kMinSamplesPerHardwarePacket = 1024;

// The maximum number of samples in a hardware packet.
// This value is selected so that we can handle up to 192khz sample rate.
static const int kMaxSamplesPerHardwarePacket = 64 * 1024;

// This constant governs the hardware audio buffer size, this value should be
// chosen carefully.
// This value is selected so that we have 8192 samples for 48khz streams.
static const int kMillisecondsPerHardwarePacket = 170;

uint32 SelectSamplesPerPacket(const AudioParameters& params) {
  // Select the number of samples that can provide at least
  // |kMillisecondsPerHardwarePacket| worth of audio data.
  int samples = kMinSamplesPerHardwarePacket;
  while (samples <= kMaxSamplesPerHardwarePacket &&
         samples * base::Time::kMillisecondsPerSecond <
         params.sample_rate * kMillisecondsPerHardwarePacket) {
    samples *= 2;
  }
  return samples;
}
