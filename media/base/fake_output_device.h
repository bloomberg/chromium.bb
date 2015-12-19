// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_FAKE_OUTPUT_DEVICE_H_
#define MEDIA_BASE_FAKE_OUTPUT_DEVICE_H_

#include <string>

#include "base/macros.h"
#include "media/base/output_device.h"

namespace media {

class FakeOutputDevice : public OutputDevice {
 public:
  FakeOutputDevice();
  explicit FakeOutputDevice(OutputDeviceStatus status);
  ~FakeOutputDevice() override;

  // OutputDevice implementation.
  void SwitchOutputDevice(const std::string& device_id,
                          const url::Origin& security_origin,
                          const SwitchOutputDeviceCB& callback) override;
  AudioParameters GetOutputParameters() override;
  OutputDeviceStatus GetDeviceStatus() override;

 private:
  OutputDeviceStatus device_status_;
  DISALLOW_COPY_AND_ASSIGN(FakeOutputDevice);
};

}  // namespace media

#endif  // MEDIA_BASE_FAKE_OUTPUT_DEVICE_H_
