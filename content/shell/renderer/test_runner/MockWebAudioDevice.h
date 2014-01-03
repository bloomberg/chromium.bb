// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBAUDIODEVICE_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBAUDIODEVICE_H_

#include "base/basictypes.h"
#include "content/shell/renderer/test_runner/TestCommon.h"
#include "third_party/WebKit/public/platform/WebAudioDevice.h"

namespace WebTestRunner {

class MockWebAudioDevice : public blink::WebAudioDevice {
public:
    explicit MockWebAudioDevice(double sampleRate);
    virtual ~MockWebAudioDevice();

    virtual void start();
    virtual void stop();
    virtual double sampleRate();

private:
    double m_sampleRate;

    DISALLOW_COPY_AND_ASSIGN(MockWebAudioDevice);
};

} // namespace WebTestRunner

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBAUDIODEVICE_H_
