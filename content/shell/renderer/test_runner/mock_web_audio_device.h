// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_WEB_AUDIO_DEVICE_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_WEB_AUDIO_DEVICE_H_

#include "base/macros.h"
#include "third_party/WebKit/public/platform/WebAudioDevice.h"

namespace content {

class MockWebAudioDevice : public blink::WebAudioDevice {
 public:
  explicit MockWebAudioDevice(double sample_rate);
  virtual ~MockWebAudioDevice();

  // blink::WebAudioDevice:
  virtual void start();
  virtual void stop();
  virtual double sampleRate();

 private:
  double sample_rate_;

  DISALLOW_COPY_AND_ASSIGN(MockWebAudioDevice);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_WEB_AUDIO_DEVICE_H_
