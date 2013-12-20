// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/MockWebAudioDevice.h"

using namespace blink;

namespace WebTestRunner {

MockWebAudioDevice::MockWebAudioDevice(double sampleRate)
    : m_sampleRate(sampleRate)
{
}

MockWebAudioDevice::~MockWebAudioDevice()
{
}

void MockWebAudioDevice::start()
{
}

void MockWebAudioDevice::stop()
{
}

double MockWebAudioDevice::sampleRate()
{
    return m_sampleRate;
}

} // namespace WebTestRunner
