// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_MOCK_WEB_AUDIO_DEVICE_H_
#define COMPONENTS_TEST_RUNNER_MOCK_WEB_AUDIO_DEVICE_H_

#include "base/macros.h"
#include "third_party/WebKit/public/platform/WebAudioDevice.h"

namespace test_runner {

class MockWebAudioDevice : public blink::WebAudioDevice {
 public:
  explicit MockWebAudioDevice(double sample_rate);
  ~MockWebAudioDevice() override;

  // blink::WebAudioDevice:
  void start() override;
  void stop() override;
  double sampleRate() override;

 private:
  double sample_rate_;

  DISALLOW_COPY_AND_ASSIGN(MockWebAudioDevice);
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_MOCK_WEB_AUDIO_DEVICE_H_
