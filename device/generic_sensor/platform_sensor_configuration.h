// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_CONFIGURATION_H_
#define DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_CONFIGURATION_H_

#include "base/logging.h"
#include "device/generic_sensor/sensor_export.h"

namespace device {

class DEVICE_SENSOR_EXPORT PlatformSensorConfiguration {
 public:
  PlatformSensorConfiguration();
  explicit PlatformSensorConfiguration(double frequency);
  ~PlatformSensorConfiguration();

  bool operator==(const PlatformSensorConfiguration& other) const;

  void set_frequency(double frequency) {
    DCHECK(frequency_ <= kMaxAllowedFrequency && frequency_ > 0.0);
    frequency_ = frequency;
  }

  double frequency() const { return frequency_; }

  static constexpr double kMaxAllowedFrequency = 60.0;

 private:
  double frequency_ = 1.0;  // 1 Hz by default.
};

}  // namespace device

#endif  // DEVICE_SENSORS_PLATFORM_SENSOR_CONFIGURATION_H_
