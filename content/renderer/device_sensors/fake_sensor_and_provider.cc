// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/device_sensors/fake_sensor_and_provider.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

FakeSensor::FakeSensor(device::mojom::SensorType sensor_type)
    : sensor_type_(sensor_type) {
  shared_buffer_handle_ = mojo::SharedBufferHandle::Create(
      sizeof(device::SensorReadingSharedBuffer) *
      static_cast<uint64_t>(device::mojom::SensorType::LAST));

  if (!shared_buffer_handle_.is_valid())
    return;

  // Create read/write mapping now, to ensure it is kept writable
  // after the region is sealed read-only on Android.
  shared_buffer_mapping_ = shared_buffer_handle_->MapAtOffset(
      device::mojom::SensorInitParams::kReadBufferSizeForTests,
      GetBufferOffset());
}

FakeSensor::~FakeSensor() = default;

void FakeSensor::AddConfiguration(
    const device::PlatformSensorConfiguration& configuration,
    AddConfigurationCallback callback) {
  std::move(callback).Run(true);
}

void FakeSensor::GetDefaultConfiguration(
    GetDefaultConfigurationCallback callback) {
  std::move(callback).Run(GetDefaultConfiguration());
}

void FakeSensor::RemoveConfiguration(
    const device::PlatformSensorConfiguration& configuration) {}

void FakeSensor::Suspend() {}

void FakeSensor::Resume() {}

void FakeSensor::ConfigureReadingChangeNotifications(bool enabled) {
  reading_notification_enabled_ = enabled;
}

device::PlatformSensorConfiguration FakeSensor::GetDefaultConfiguration() {
  return device::PlatformSensorConfiguration(60 /* frequency */);
}

device::mojom::ReportingMode FakeSensor::GetReportingMode() {
  return device::mojom::ReportingMode::ON_CHANGE;
}

double FakeSensor::GetMaximumSupportedFrequency() {
  return 60.0;
}

double FakeSensor::GetMinimumSupportedFrequency() {
  return 1.0;
}

device::mojom::SensorClientRequest FakeSensor::GetClient() {
  return mojo::MakeRequest(&client_);
}

mojo::ScopedSharedBufferHandle FakeSensor::GetSharedBufferHandle() {
  return shared_buffer_handle_->Clone(
      mojo::SharedBufferHandle::AccessMode::READ_ONLY);
}

uint64_t FakeSensor::GetBufferOffset() {
  return device::SensorReadingSharedBuffer::GetOffset(sensor_type_);
}

void FakeSensor::SetReading(device::SensorReading reading) {
  reading_ = reading;
  SensorReadingChanged();
}

void FakeSensor::SensorReadingChanged() {
  if (!shared_buffer_mapping_.get())
    return;

  auto* buffer = static_cast<device::SensorReadingSharedBuffer*>(
      shared_buffer_mapping_.get());

  auto& seqlock = buffer->seqlock.value();
  seqlock.WriteBegin();
  buffer->reading = reading_;
  seqlock.WriteEnd();

  if (client_ && reading_notification_enabled_)
    client_->SensorReadingChanged();
}

FakeSensorProvider::FakeSensorProvider() : binding_(this) {}

FakeSensorProvider::~FakeSensorProvider() = default;

void FakeSensorProvider::GetSensor(device::mojom::SensorType type,
                                   GetSensorCallback callback) {
  std::unique_ptr<FakeSensor> sensor;

  switch (type) {
    case device::mojom::SensorType::ACCELEROMETER:
      if (accelerometer_is_available_) {
        sensor = std::make_unique<FakeSensor>(
            device::mojom::SensorType::ACCELEROMETER);
        accelerometer_ = sensor.get();
      }
      break;
    case device::mojom::SensorType::LINEAR_ACCELERATION:
      if (linear_acceleration_sensor_is_available_) {
        sensor = std::make_unique<FakeSensor>(
            device::mojom::SensorType::LINEAR_ACCELERATION);
        linear_acceleration_sensor_ = sensor.get();
      }
      break;
    case device::mojom::SensorType::GYROSCOPE:
      if (gyroscope_is_available_) {
        sensor =
            std::make_unique<FakeSensor>(device::mojom::SensorType::GYROSCOPE);
        gyroscope_ = sensor.get();
      }
      break;
    case device::mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES:
      if (relative_orientation_sensor_is_available_) {
        sensor = std::make_unique<FakeSensor>(
            device::mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES);
        relative_orientation_sensor_ = sensor.get();
      }
      break;
    case device::mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES:
      if (absolute_orientation_sensor_is_available_) {
        sensor = std::make_unique<FakeSensor>(
            device::mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES);
        absolute_orientation_sensor_ = sensor.get();
      }
      break;
    default:
      NOTIMPLEMENTED();
  }

  if (sensor) {
    auto init_params = device::mojom::SensorInitParams::New();
    init_params->client_request = sensor->GetClient();
    init_params->memory = sensor->GetSharedBufferHandle();
    init_params->buffer_offset = sensor->GetBufferOffset();
    init_params->default_configuration = sensor->GetDefaultConfiguration();
    init_params->maximum_frequency = sensor->GetMaximumSupportedFrequency();
    init_params->minimum_frequency = sensor->GetMinimumSupportedFrequency();

    mojo::MakeStrongBinding(std::move(sensor),
                            mojo::MakeRequest(&init_params->sensor));
    std::move(callback).Run(std::move(init_params));
  } else {
    std::move(callback).Run(nullptr);
  }
}

void FakeSensorProvider::Bind(device::mojom::SensorProviderRequest request) {
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(request));
}

void FakeSensorProvider::SetAccelerometerData(double x, double y, double z) {
  device::SensorReading reading;
  reading.raw.timestamp =
      (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
  reading.accel.x = x;
  reading.accel.y = y;
  reading.accel.z = z;
  EXPECT_TRUE(accelerometer_);
  accelerometer_->SetReading(reading);
}

void FakeSensorProvider::SetLinearAccelerationSensorData(double x,
                                                         double y,
                                                         double z) {
  device::SensorReading reading;
  reading.raw.timestamp =
      (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
  reading.accel.x = x;
  reading.accel.y = y;
  reading.accel.z = z;
  EXPECT_TRUE(linear_acceleration_sensor_);
  linear_acceleration_sensor_->SetReading(reading);
}

void FakeSensorProvider::SetGyroscopeData(double x, double y, double z) {
  device::SensorReading reading;
  reading.raw.timestamp =
      (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
  reading.gyro.x = x;
  reading.gyro.y = y;
  reading.gyro.z = z;
  EXPECT_TRUE(gyroscope_);
  gyroscope_->SetReading(reading);
}

void FakeSensorProvider::SetRelativeOrientationSensorData(double alpha,
                                                          double beta,
                                                          double gamma) {
  device::SensorReading reading;
  reading.raw.timestamp =
      (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
  reading.orientation_euler.x = beta;
  reading.orientation_euler.y = gamma;
  reading.orientation_euler.z = alpha;
  EXPECT_TRUE(relative_orientation_sensor_);
  relative_orientation_sensor_->SetReading(reading);
}

void FakeSensorProvider::SetAbsoluteOrientationSensorData(double alpha,
                                                          double beta,
                                                          double gamma) {
  device::SensorReading reading;
  reading.raw.timestamp =
      (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
  reading.orientation_euler.x = beta;
  reading.orientation_euler.y = gamma;
  reading.orientation_euler.z = alpha;
  EXPECT_TRUE(absolute_orientation_sensor_);
  absolute_orientation_sensor_->SetReading(reading);
}

}  // namespace content
