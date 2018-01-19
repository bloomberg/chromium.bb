// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DEVICE_SENSORS_FAKE_SENSOR_AND_PROVIDER_H_
#define CONTENT_RENDERER_DEVICE_SENSORS_FAKE_SENSOR_AND_PROVIDER_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/buffer.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/interfaces/sensor.mojom.h"
#include "services/device/public/interfaces/sensor_provider.mojom.h"

// TODO(juncai): Move this file in a new
// //services/device/public/cpp/generic_sensor:test_support source_set and
// share it with device_sensor_browsertest.cc and generic_sensor_browsertest.cc.

namespace content {

class FakeSensor : public device::mojom::Sensor {
 public:
  FakeSensor(device::mojom::SensorType sensor_type);
  ~FakeSensor() override;

  // device::mojom::Sensor:
  void AddConfiguration(
      const device::PlatformSensorConfiguration& configuration,
      AddConfigurationCallback callback) override;
  void GetDefaultConfiguration(
      GetDefaultConfigurationCallback callback) override;
  void RemoveConfiguration(
      const device::PlatformSensorConfiguration& configuration) override;
  void Suspend() override;
  void Resume() override;
  void ConfigureReadingChangeNotifications(bool enabled) override;

  device::PlatformSensorConfiguration GetDefaultConfiguration();

  device::mojom::ReportingMode GetReportingMode();

  double GetMaximumSupportedFrequency();

  double GetMinimumSupportedFrequency();

  device::mojom::SensorClientRequest GetClient();

  mojo::ScopedSharedBufferHandle GetSharedBufferHandle();

  uint64_t GetBufferOffset();

  void SetReading(device::SensorReading reading);

 private:
  void SensorReadingChanged();

  device::mojom::SensorType sensor_type_;
  bool reading_notification_enabled_ = true;
  mojo::ScopedSharedBufferHandle shared_buffer_handle_;
  mojo::ScopedSharedBufferMapping shared_buffer_mapping_;
  device::mojom::SensorClientPtr client_;
  device::SensorReading reading_;

  DISALLOW_COPY_AND_ASSIGN(FakeSensor);
};

class FakeSensorProvider : public device::mojom::SensorProvider {
 public:
  FakeSensorProvider();
  ~FakeSensorProvider() override;

  // device::mojom::sensorProvider:
  void GetSensor(device::mojom::SensorType type,
                 GetSensorCallback callback) override;

  void Bind(device::mojom::SensorProviderRequest request);

  void set_accelerometer_is_available(bool accelerometer_is_available) {
    accelerometer_is_available_ = accelerometer_is_available;
  }

  void set_linear_acceleration_sensor_is_available(
      bool linear_acceleration_sensor_is_available) {
    linear_acceleration_sensor_is_available_ =
        linear_acceleration_sensor_is_available;
  }

  void set_gyroscope_is_available(bool gyroscope_is_available) {
    gyroscope_is_available_ = gyroscope_is_available;
  }

  void set_relative_orientation_sensor_is_available(
      bool relative_orientation_sensor_is_available) {
    relative_orientation_sensor_is_available_ =
        relative_orientation_sensor_is_available;
  }

  void set_absolute_orientation_sensor_is_available(
      bool absolute_orientation_sensor_is_available) {
    absolute_orientation_sensor_is_available_ =
        absolute_orientation_sensor_is_available;
  }

  void SetAccelerometerData(double x, double y, double z);

  void SetLinearAccelerationSensorData(double x, double y, double z);

  void SetGyroscopeData(double x, double y, double z);

  void SetRelativeOrientationSensorData(double alpha,
                                        double beta,
                                        double gamma);

  void SetAbsoluteOrientationSensorData(double alpha,
                                        double beta,
                                        double gamma);

 private:
  FakeSensor* accelerometer_ = nullptr;
  FakeSensor* linear_acceleration_sensor_ = nullptr;
  FakeSensor* gyroscope_ = nullptr;
  FakeSensor* relative_orientation_sensor_ = nullptr;
  FakeSensor* absolute_orientation_sensor_ = nullptr;
  bool accelerometer_is_available_ = true;
  bool linear_acceleration_sensor_is_available_ = true;
  bool gyroscope_is_available_ = true;
  bool relative_orientation_sensor_is_available_ = true;
  bool absolute_orientation_sensor_is_available_ = true;
  mojo::Binding<device::mojom::SensorProvider> binding_;

  DISALLOW_COPY_AND_ASSIGN(FakeSensorProvider);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DEVICE_SENSORS_FAKE_SENSOR_AND_PROVIDER_H_
