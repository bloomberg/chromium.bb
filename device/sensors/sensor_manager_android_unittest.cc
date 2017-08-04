// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/sensors/sensor_manager_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "device/sensors/device_sensors_consts.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

class FakeSensorManagerAndroid : public SensorManagerAndroid {
 public:
  FakeSensorManagerAndroid() {}
  ~FakeSensorManagerAndroid() override {}

  OrientationSensorType GetOrientationSensorTypeUsed() override {
    return SensorManagerAndroid::ROTATION_VECTOR;
  }

  int GetNumberActiveDeviceMotionSensors() override {
    return number_active_sensors_;
  }

  void SetNumberActiveDeviceMotionSensors(int number_active_sensors) {
    number_active_sensors_ = number_active_sensors;
  }

 protected:
  bool Start(ConsumerType event_type) override { return true; }
  void Stop(ConsumerType event_type) override {}

 private:
  int number_active_sensors_;
};

class AndroidSensorManagerTest : public testing::Test {
 protected:
  AndroidSensorManagerTest() {
    motion_buffer_.reset(new DeviceMotionHardwareBuffer);
    orientation_buffer_.reset(new DeviceOrientationHardwareBuffer);
    orientation_absolute_buffer_.reset(new DeviceOrientationHardwareBuffer);
  }

  void VerifyOrientationBufferValues(
      const DeviceOrientationHardwareBuffer* buffer,
      double alpha,
      double beta,
      double gamma) {
    ASSERT_TRUE(buffer->data.all_available_sensors_are_active);
    ASSERT_EQ(alpha, buffer->data.alpha);
    ASSERT_TRUE(buffer->data.has_alpha);
    ASSERT_EQ(beta, buffer->data.beta);
    ASSERT_TRUE(buffer->data.has_beta);
    ASSERT_EQ(gamma, buffer->data.gamma);
    ASSERT_TRUE(buffer->data.has_gamma);
  }

  std::unique_ptr<DeviceMotionHardwareBuffer> motion_buffer_;
  std::unique_ptr<DeviceOrientationHardwareBuffer> orientation_buffer_;
  std::unique_ptr<DeviceOrientationHardwareBuffer> orientation_absolute_buffer_;
  base::MessageLoop message_loop_;
};

TEST_F(AndroidSensorManagerTest, ThreeDeviceMotionSensorsActive) {
  FakeSensorManagerAndroid sensorManager;
  sensorManager.SetNumberActiveDeviceMotionSensors(3);

  sensorManager.StartFetchingDeviceMotionData(motion_buffer_.get());
  ASSERT_FALSE(motion_buffer_->data.all_available_sensors_are_active);

  sensorManager.GotAcceleration(nullptr, nullptr, 1, 2, 3);
  ASSERT_FALSE(motion_buffer_->data.all_available_sensors_are_active);
  ASSERT_EQ(1, motion_buffer_->data.acceleration_x);
  ASSERT_TRUE(motion_buffer_->data.has_acceleration_x);
  ASSERT_EQ(2, motion_buffer_->data.acceleration_y);
  ASSERT_TRUE(motion_buffer_->data.has_acceleration_y);
  ASSERT_EQ(3, motion_buffer_->data.acceleration_z);
  ASSERT_TRUE(motion_buffer_->data.has_acceleration_z);

  sensorManager.GotAccelerationIncludingGravity(nullptr, nullptr, 4, 5, 6);
  ASSERT_FALSE(motion_buffer_->data.all_available_sensors_are_active);
  ASSERT_EQ(4, motion_buffer_->data.acceleration_including_gravity_x);
  ASSERT_TRUE(motion_buffer_->data.has_acceleration_including_gravity_x);
  ASSERT_EQ(5, motion_buffer_->data.acceleration_including_gravity_y);
  ASSERT_TRUE(motion_buffer_->data.has_acceleration_including_gravity_y);
  ASSERT_EQ(6, motion_buffer_->data.acceleration_including_gravity_z);
  ASSERT_TRUE(motion_buffer_->data.has_acceleration_including_gravity_z);

  sensorManager.GotRotationRate(nullptr, nullptr, 7, 8, 9);
  ASSERT_TRUE(motion_buffer_->data.all_available_sensors_are_active);
  ASSERT_EQ(7, motion_buffer_->data.rotation_rate_alpha);
  ASSERT_TRUE(motion_buffer_->data.has_rotation_rate_alpha);
  ASSERT_EQ(8, motion_buffer_->data.rotation_rate_beta);
  ASSERT_TRUE(motion_buffer_->data.has_rotation_rate_beta);
  ASSERT_EQ(9, motion_buffer_->data.rotation_rate_gamma);
  ASSERT_TRUE(motion_buffer_->data.has_rotation_rate_gamma);
  ASSERT_EQ(kDeviceSensorIntervalMicroseconds / 1000.,
            motion_buffer_->data.interval);

  sensorManager.StopFetchingDeviceMotionData();
  ASSERT_FALSE(motion_buffer_->data.all_available_sensors_are_active);
}

TEST_F(AndroidSensorManagerTest, TwoDeviceMotionSensorsActive) {
  FakeSensorManagerAndroid sensorManager;
  sensorManager.SetNumberActiveDeviceMotionSensors(2);

  sensorManager.StartFetchingDeviceMotionData(motion_buffer_.get());
  ASSERT_FALSE(motion_buffer_->data.all_available_sensors_are_active);

  sensorManager.GotAcceleration(nullptr, nullptr, 1, 2, 3);
  ASSERT_FALSE(motion_buffer_->data.all_available_sensors_are_active);

  sensorManager.GotAccelerationIncludingGravity(nullptr, nullptr, 1, 2, 3);
  ASSERT_TRUE(motion_buffer_->data.all_available_sensors_are_active);
  ASSERT_EQ(kDeviceSensorIntervalMicroseconds / 1000.,
            motion_buffer_->data.interval);

  sensorManager.StopFetchingDeviceMotionData();
  ASSERT_FALSE(motion_buffer_->data.all_available_sensors_are_active);
}

TEST_F(AndroidSensorManagerTest, ZeroDeviceMotionSensorsActive) {
  FakeSensorManagerAndroid sensorManager;
  sensorManager.SetNumberActiveDeviceMotionSensors(0);

  sensorManager.StartFetchingDeviceMotionData(motion_buffer_.get());
  ASSERT_TRUE(motion_buffer_->data.all_available_sensors_are_active);
  ASSERT_EQ(kDeviceSensorIntervalMicroseconds / 1000.,
            motion_buffer_->data.interval);

  sensorManager.StopFetchingDeviceMotionData();
  ASSERT_FALSE(motion_buffer_->data.all_available_sensors_are_active);
}

TEST_F(AndroidSensorManagerTest, DeviceOrientationSensorsActive) {
  FakeSensorManagerAndroid sensorManager;

  sensorManager.StartFetchingDeviceOrientationData(orientation_buffer_.get());
  ASSERT_FALSE(orientation_buffer_->data.all_available_sensors_are_active);

  sensorManager.GotOrientation(nullptr, nullptr, 1, 2, 3);
  VerifyOrientationBufferValues(orientation_buffer_.get(), 1, 2, 3);

  sensorManager.StopFetchingDeviceOrientationData();
  ASSERT_FALSE(orientation_buffer_->data.all_available_sensors_are_active);
}

TEST_F(AndroidSensorManagerTest, DeviceOrientationAbsoluteSensorsActive) {
  FakeSensorManagerAndroid sensorManager;

  sensorManager.StartFetchingDeviceOrientationAbsoluteData(
      orientation_absolute_buffer_.get());
  ASSERT_FALSE(orientation_buffer_->data.all_available_sensors_are_active);

  sensorManager.GotOrientationAbsolute(nullptr, nullptr, 4, 5, 6);
  VerifyOrientationBufferValues(orientation_absolute_buffer_.get(), 4, 5, 6);

  sensorManager.StopFetchingDeviceOrientationData();
  ASSERT_FALSE(orientation_buffer_->data.all_available_sensors_are_active);
}

}  // namespace

}  // namespace device
