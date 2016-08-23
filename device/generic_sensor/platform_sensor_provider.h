// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_H_
#define DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_H_

#include "device/generic_sensor/platform_sensor_provider_base.h"

namespace device {

// This a singleton class returning the actual sensor provider
// implementation for the current platform.
class PlatformSensorProvider : public PlatformSensorProviderBase {
 public:
  // Returns the PlatformSensorProvider singleton.
  // Note: returns 'nullptr' if there is no available implementation for
  // the current platform.
  static PlatformSensorProvider* GetInstance();

 protected:
  PlatformSensorProvider() = default;
  ~PlatformSensorProvider() override = default;

  DISALLOW_COPY_AND_ASSIGN(PlatformSensorProvider);
};

}  // namespace device

#endif  // DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_H_
