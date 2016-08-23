// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/generic_sensor/platform_sensor_configuration.h"

namespace device {

PlatformSensorConfiguration::PlatformSensorConfiguration(double frequency)
    : frequency_(frequency) {
  DCHECK(frequency_ <= kMaxAllowedFrequency && frequency_ > 0.0);
}

PlatformSensorConfiguration::PlatformSensorConfiguration() = default;
PlatformSensorConfiguration::~PlatformSensorConfiguration() = default;

bool PlatformSensorConfiguration::operator==(
    const PlatformSensorConfiguration& other) const {
  return frequency_ == other.frequency();
}

}  // namespace device
