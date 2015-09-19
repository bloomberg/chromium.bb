// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/fake_output_device.h"

#include "base/callback.h"

namespace media {

FakeOutputDevice::FakeOutputDevice() {}
FakeOutputDevice::~FakeOutputDevice() {}

void FakeOutputDevice::SwitchOutputDevice(
    const std::string& device_id,
    const url::Origin& security_origin,
    const SwitchOutputDeviceCB& callback) {
  callback.Run(SWITCH_OUTPUT_DEVICE_RESULT_SUCCESS);
}

AudioParameters FakeOutputDevice::GetOutputParameters() {
  return media::AudioParameters(
      media::AudioParameters::AUDIO_FAKE, media::CHANNEL_LAYOUT_STEREO,
      media::AudioParameters::kTelephoneSampleRate, 16, 1);
}

}  // namespace media
