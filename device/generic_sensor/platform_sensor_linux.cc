// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/generic_sensor/platform_sensor_linux.h"

#include "device/generic_sensor/linux/sensor_data_linux.h"
#include "device/generic_sensor/platform_sensor_reader_linux.h"

namespace device {

namespace {

// Checks if at least one value has been changed.
bool HaveValuesChanged(const SensorReading& lhs, const SensorReading& rhs) {
  return lhs.values[0] != rhs.values[0] || lhs.values[1] != rhs.values[1] ||
         lhs.values[2] != rhs.values[2];
}

}  // namespace

PlatformSensorLinux::PlatformSensorLinux(
    mojom::SensorType type,
    mojo::ScopedSharedBufferMapping mapping,
    PlatformSensorProvider* provider,
    const SensorInfoLinux* sensor_device,
    scoped_refptr<base::SingleThreadTaskRunner> polling_thread_task_runner)
    : PlatformSensor(type, std::move(mapping), provider),
      default_configuration_(
          PlatformSensorConfiguration(sensor_device->device_frequency)),
      reporting_mode_(sensor_device->reporting_mode),
      weak_factory_(this) {
  sensor_reader_ =
      SensorReader::Create(sensor_device, this, polling_thread_task_runner);
}

PlatformSensorLinux::~PlatformSensorLinux() = default;

mojom::ReportingMode PlatformSensorLinux::GetReportingMode() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return reporting_mode_;
}

void PlatformSensorLinux::UpdatePlatformSensorReading(SensorReading reading) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  bool notifyNeeded = false;
  if (GetReportingMode() == mojom::ReportingMode::ON_CHANGE) {
    if (!HaveValuesChanged(reading, old_values_))
      return;
    notifyNeeded = true;
  }
  old_values_ = reading;
  reading.timestamp = (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
  UpdateSensorReading(reading, notifyNeeded);
}

void PlatformSensorLinux::NotifyPlatformSensorError() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  NotifySensorError();
}

bool PlatformSensorLinux::StartSensor(
    const PlatformSensorConfiguration& configuration) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!sensor_reader_)
    return false;
  return sensor_reader_->StartFetchingData(configuration);
}

void PlatformSensorLinux::StopSensor() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(sensor_reader_);
  sensor_reader_->StopFetchingData();
}

bool PlatformSensorLinux::CheckSensorConfiguration(
    const PlatformSensorConfiguration& configuration) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return configuration.frequency() > 0 &&
         configuration.frequency() <= default_configuration_.frequency();
}

PlatformSensorConfiguration PlatformSensorLinux::GetDefaultConfiguration() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return default_configuration_;
}

}  // namespace device
