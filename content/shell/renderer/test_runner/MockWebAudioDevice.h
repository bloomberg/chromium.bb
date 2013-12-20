// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MockWebAudioDevice_h
#define MockWebAudioDevice_h

#include "content/shell/renderer/test_runner/TestCommon.h"
#include "third_party/WebKit/public/platform/WebAudioDevice.h"
#include "third_party/WebKit/public/platform/WebNonCopyable.h"

namespace WebTestRunner {

class MockWebAudioDevice : public blink::WebAudioDevice, public blink::WebNonCopyable {
public:
    explicit MockWebAudioDevice(double sampleRate);
    virtual ~MockWebAudioDevice();

    virtual void start();
    virtual void stop();
    virtual double sampleRate();

private:
    double m_sampleRate;
};

} // namespace WebTestRunner

#endif // MockWebAudioDevice_h
