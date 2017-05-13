// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/generic_sensor/platform_sensor_accelerometer_mac.h"

#include <stdint.h>

#include <cmath>

#include "base/bind.h"
#include "device/base/synchronization/shared_memory_seqlock_buffer.h"
#include "device/generic_sensor/generic_sensor_consts.h"
#include "device/generic_sensor/platform_sensor_provider_mac.h"
#include "third_party/sudden_motion_sensor/sudden_motion_sensor_mac.h"

namespace {

constexpr double kMeanGravity = 9.80665;

constexpr double kGravityThreshold = kMeanGravity * 0.01;

bool IsSignificantlyDifferent(const device::SensorReading& reading1,
                              const device::SensorReading& reading2) {
  return (std::fabs(reading1.values[0] - reading2.values[0]) >=
          kGravityThreshold) ||
         (std::fabs(reading1.values[1] - reading2.values[1]) >=
          kGravityThreshold) ||
         (std::fabs(reading1.values[2] - reading2.values[2]) >=
          kGravityThreshold);
}

}  // namespace

namespace device {

PlatformSensorAccelerometerMac::PlatformSensorAccelerometerMac(
    mojo::ScopedSharedBufferMapping mapping,
    PlatformSensorProvider* provider)
    : PlatformSensor(mojom::SensorType::ACCELEROMETER,
                     std::move(mapping),
                     provider),
      sudden_motion_sensor_(SuddenMotionSensor::Create()) {}

PlatformSensorAccelerometerMac::~PlatformSensorAccelerometerMac() = default;

mojom::ReportingMode PlatformSensorAccelerometerMac::GetReportingMode() {
  return mojom::ReportingMode::ON_CHANGE;
}

bool PlatformSensorAccelerometerMac::CheckSensorConfiguration(
    const PlatformSensorConfiguration& configuration) {
  return configuration.frequency() > 0 &&
         configuration.frequency() <=
             mojom::SensorConfiguration::kMaxAllowedFrequency;
}

PlatformSensorConfiguration
PlatformSensorAccelerometerMac::GetDefaultConfiguration() {
  return PlatformSensorConfiguration(kDefaultAccelerometerFrequencyHz);
}

bool PlatformSensorAccelerometerMac::StartSensor(
    const PlatformSensorConfiguration& configuration) {
  if (!sudden_motion_sensor_)
    return false;

  float axis_value[3];
  if (!sudden_motion_sensor_->ReadSensorValues(axis_value))
    return false;

  timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMicroseconds(base::Time::kMicrosecondsPerSecond /
                                        configuration.frequency()),
      this, &PlatformSensorAccelerometerMac::PollForData);

  return true;
}

void PlatformSensorAccelerometerMac::StopSensor() {
  timer_.Stop();
}

void PlatformSensorAccelerometerMac::PollForData() {
  // Retrieve per-axis calibrated values.
  float axis_value[3];
  if (!sudden_motion_sensor_->ReadSensorValues(axis_value))
    return;

  SensorReading reading;
  reading.timestamp = (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
  reading.values[0] = axis_value[0] * kMeanGravity;
  reading.values[1] = axis_value[1] * kMeanGravity;
  reading.values[2] = axis_value[2] * kMeanGravity;

  if (IsSignificantlyDifferent(reading_, reading)) {
    reading_ = reading;
    UpdateSensorReading(reading, true);
  }
}

}  // namespace device
