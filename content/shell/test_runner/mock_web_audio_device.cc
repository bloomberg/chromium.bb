// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/mock_web_audio_device.h"

namespace test_runner {

MockWebAudioDevice::MockWebAudioDevice(double sample_rate,
                                       int frames_per_buffer)
    : sample_rate_(sample_rate), frames_per_buffer_(frames_per_buffer) {}

MockWebAudioDevice::~MockWebAudioDevice() {}

void MockWebAudioDevice::start() {}

void MockWebAudioDevice::stop() {}

double MockWebAudioDevice::sampleRate() {
  return sample_rate_;
}

int MockWebAudioDevice::framesPerBuffer() {
  return frames_per_buffer_;
}

}  // namespace test_runner
