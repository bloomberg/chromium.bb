// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "media/cast/test/utility/audio_utility.h"

const size_t kSamplingFrequency = 48000;

int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "Usage: %s <fps> <frames> >output.s16le\n", argv[0]);
    exit(1);
  }
  int fps = atoi(argv[1]);
  int frames = atoi(argv[2]);
  std::vector<int16> samples(kSamplingFrequency / fps);
  size_t num_samples = 0;
  for (uint32 frame_id = 1; frame_id <= frames; frame_id++) {
    CHECK(media::cast::EncodeTimestamp(frame_id, num_samples, &samples));
    num_samples += samples.size();
    for (size_t sample = 0; sample < samples.size(); sample++) {
      putchar(samples[sample] & 0xff);
      putchar(samples[sample] >> 8);
      putchar(samples[sample] & 0xff);
      putchar(samples[sample] >> 8);
    }
  }
}
