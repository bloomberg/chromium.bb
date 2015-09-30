// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mock_audio_renderer_sink.h"
#include "media/base/fake_output_device.h"

namespace media {
MockAudioRendererSink::MockAudioRendererSink()
    : MockAudioRendererSink(OUTPUT_DEVICE_STATUS_OK) {}

MockAudioRendererSink::MockAudioRendererSink(OutputDeviceStatus device_status)
    : output_device_(new FakeOutputDevice(device_status)) {}

MockAudioRendererSink::~MockAudioRendererSink() {}

void MockAudioRendererSink::Initialize(const AudioParameters& params,
                                       RenderCallback* renderer) {
  callback_ = renderer;
}

OutputDevice* MockAudioRendererSink::GetOutputDevice() {
  return output_device_.get();
}

}  // namespace media
