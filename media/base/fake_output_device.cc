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
    const GURL& security_origin,
    const SwitchOutputDeviceCB& callback) {
  callback.Run(SWITCH_OUTPUT_DEVICE_RESULT_SUCCESS);
}

}  // namespace media
