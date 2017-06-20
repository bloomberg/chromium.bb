// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/device_sensors/device_motion_event_pump.h"

#include <string.h>

#include <memory>

#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/public/test/test_utils.h"
#include "device/generic_sensor/public/cpp/sensor_reading.h"
#include "device/generic_sensor/public/interfaces/sensor.mojom.h"
#include "device/generic_sensor/public/interfaces/sensor_provider.mojom.h"
#include "device/sensors/public/cpp/motion_data.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/system/buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/modules/device_orientation/WebDeviceMotionListener.h"

namespace {

constexpr uint64_t kReadingBufferSize =
    sizeof(device::SensorReadingSharedBuffer);

constexpr uint64_t kSharedBufferSizeInBytes =
    kReadingBufferSize * static_cast<uint64_t>(device::mojom::SensorType::LAST);

}  // namespace

namespace content {

class MockDeviceMotionListener : public blink::WebDeviceMotionListener {
 public:
  MockDeviceMotionListener()
      : did_change_device_motion_(false), number_of_events_(0) {
    memset(&data_, 0, sizeof(data_));
  }
  ~MockDeviceMotionListener() override {}

  void DidChangeDeviceMotion(const device::MotionData& data) override {
    memcpy(&data_, &data, sizeof(data));
    did_change_device_motion_ = true;
    ++number_of_events_;
  }

  bool did_change_device_motion() const {
    return did_change_device_motion_;
  }

  int number_of_events() const { return number_of_events_; }

  const device::MotionData& data() const { return data_; }

 private:
  bool did_change_device_motion_;
  int number_of_events_;
  device::MotionData data_;

  DISALLOW_COPY_AND_ASSIGN(MockDeviceMotionListener);
};

class DeviceMotionEventPumpForTesting : public DeviceMotionEventPump {
 public:
  DeviceMotionEventPumpForTesting()
      : DeviceMotionEventPump(nullptr), stop_on_fire_event_(true) {}
  ~DeviceMotionEventPumpForTesting() override {}

  // DeviceMotionEventPump:
  void SendStartMessage() override {
    accelerometer_.mode = device::mojom::ReportingMode::CONTINUOUS;
    linear_acceleration_sensor_.mode = device::mojom::ReportingMode::ON_CHANGE;
    gyroscope_.mode = device::mojom::ReportingMode::CONTINUOUS;

    shared_memory_ = mojo::SharedBufferHandle::Create(kSharedBufferSizeInBytes);

    accelerometer_.shared_buffer = shared_memory_->MapAtOffset(
        kReadingBufferSize,
        device::SensorReadingSharedBuffer::GetOffset(accelerometer_.type));
    accelerometer_buffer_ = static_cast<device::SensorReadingSharedBuffer*>(
        accelerometer_.shared_buffer.get());

    linear_acceleration_sensor_.shared_buffer = shared_memory_->MapAtOffset(
        kReadingBufferSize, device::SensorReadingSharedBuffer::GetOffset(
                                linear_acceleration_sensor_.type));
    linear_acceleration_sensor_buffer_ =
        static_cast<device::SensorReadingSharedBuffer*>(
            linear_acceleration_sensor_.shared_buffer.get());

    gyroscope_.shared_buffer = shared_memory_->MapAtOffset(
        kReadingBufferSize,
        device::SensorReadingSharedBuffer::GetOffset(gyroscope_.type));
    gyroscope_buffer_ = static_cast<device::SensorReadingSharedBuffer*>(
        gyroscope_.shared_buffer.get());
  }

  void StartFireEvent() { DeviceMotionEventPump::DidStart(); }

  void SetAccelerometerSensorData(bool active,
                                  double d0,
                                  double d1,
                                  double d2) {
    if (active) {
      mojo::MakeRequest(&accelerometer_.sensor);
      accelerometer_buffer_->reading.timestamp =
          (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
      accelerometer_buffer_->reading.values[0].value() = d0;
      accelerometer_buffer_->reading.values[1].value() = d1;
      accelerometer_buffer_->reading.values[2].value() = d2;
    } else {
      accelerometer_.sensor.reset();
    }
  }

  void SetLinearAccelerationSensorData(bool active,
                                       double d0,
                                       double d1,
                                       double d2) {
    if (active) {
      mojo::MakeRequest(&linear_acceleration_sensor_.sensor);
      linear_acceleration_sensor_buffer_->reading.timestamp =
          (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
      linear_acceleration_sensor_buffer_->reading.values[0].value() = d0;
      linear_acceleration_sensor_buffer_->reading.values[1].value() = d1;
      linear_acceleration_sensor_buffer_->reading.values[2].value() = d2;
    } else {
      linear_acceleration_sensor_.sensor.reset();
    }
  }

  void SetGyroscopeSensorData(bool active, double d0, double d1, double d2) {
    if (active) {
      mojo::MakeRequest(&gyroscope_.sensor);
      gyroscope_buffer_->reading.timestamp =
          (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
      gyroscope_buffer_->reading.values[0].value() = d0;
      gyroscope_buffer_->reading.values[1].value() = d1;
      gyroscope_buffer_->reading.values[2].value() = d2;
    } else {
      gyroscope_.sensor.reset();
    }
  }

  void set_stop_on_fire_event(bool stop_on_fire_event) {
    stop_on_fire_event_ = stop_on_fire_event;
  }

  bool stop_on_fire_event() { return stop_on_fire_event_; }

  int pump_delay_microseconds() const { return kDefaultPumpDelayMicroseconds; }

 protected:
  // DeviceMotionEventPump:
  void FireEvent() override {
    DeviceMotionEventPump::FireEvent();
    if (stop_on_fire_event_) {
      Stop();
      base::MessageLoop::current()->QuitWhenIdle();
    }
  }

 private:
  bool stop_on_fire_event_;
  mojo::ScopedSharedBufferHandle shared_memory_;
  device::SensorReadingSharedBuffer* accelerometer_buffer_;
  device::SensorReadingSharedBuffer* linear_acceleration_sensor_buffer_;
  device::SensorReadingSharedBuffer* gyroscope_buffer_;

  DISALLOW_COPY_AND_ASSIGN(DeviceMotionEventPumpForTesting);
};

class DeviceMotionEventPumpTest : public testing::Test {
 public:
  DeviceMotionEventPumpTest() = default;

 protected:
  void SetUp() override {
    listener_.reset(new MockDeviceMotionListener);
    motion_pump_.reset(new DeviceMotionEventPumpForTesting());
  }

  MockDeviceMotionListener* listener() { return listener_.get(); }
  DeviceMotionEventPumpForTesting* motion_pump() { return motion_pump_.get(); }

 private:
  base::MessageLoop loop_;
  std::unique_ptr<MockDeviceMotionListener> listener_;
  std::unique_ptr<DeviceMotionEventPumpForTesting> motion_pump_;

  DISALLOW_COPY_AND_ASSIGN(DeviceMotionEventPumpTest);
};

TEST_F(DeviceMotionEventPumpTest, AllSensorsAreActive) {
  motion_pump()->Start(listener());
  motion_pump()->SetAccelerometerSensorData(true /* active */, 1, 2, 3);
  motion_pump()->SetLinearAccelerationSensorData(true /* active */, 4, 5, 6);
  motion_pump()->SetGyroscopeSensorData(true /* active */, 7, 8, 9);
  motion_pump()->StartFireEvent();

  base::RunLoop().Run();

  device::MotionData received_data = listener()->data();
  EXPECT_TRUE(listener()->did_change_device_motion());

  EXPECT_TRUE(received_data.has_acceleration_including_gravity_x);
  EXPECT_EQ(1, received_data.acceleration_including_gravity_x);
  EXPECT_TRUE(received_data.has_acceleration_including_gravity_y);
  EXPECT_EQ(2, received_data.acceleration_including_gravity_y);
  EXPECT_TRUE(received_data.has_acceleration_including_gravity_z);
  EXPECT_EQ(3, received_data.acceleration_including_gravity_z);

  EXPECT_TRUE(received_data.has_acceleration_x);
  EXPECT_EQ(4, received_data.acceleration_x);
  EXPECT_TRUE(received_data.has_acceleration_y);
  EXPECT_EQ(5, received_data.acceleration_y);
  EXPECT_TRUE(received_data.has_acceleration_z);
  EXPECT_EQ(6, received_data.acceleration_z);

  EXPECT_TRUE(received_data.has_rotation_rate_alpha);
  EXPECT_EQ(7, received_data.rotation_rate_alpha);
  EXPECT_TRUE(received_data.has_rotation_rate_beta);
  EXPECT_EQ(8, received_data.rotation_rate_beta);
  EXPECT_TRUE(received_data.has_rotation_rate_gamma);
  EXPECT_EQ(9, received_data.rotation_rate_gamma);
}

TEST_F(DeviceMotionEventPumpTest, TwoSensorsAreActive) {
  motion_pump()->Start(listener());
  motion_pump()->SetAccelerometerSensorData(true /* active */, 1, 2, 3);
  motion_pump()->SetLinearAccelerationSensorData(false /* active */, 4, 5, 6);
  motion_pump()->SetGyroscopeSensorData(true /* active */, 7, 8, 9);
  motion_pump()->StartFireEvent();

  base::RunLoop().Run();

  device::MotionData received_data = listener()->data();
  EXPECT_TRUE(listener()->did_change_device_motion());

  EXPECT_TRUE(received_data.has_acceleration_including_gravity_x);
  EXPECT_EQ(1, received_data.acceleration_including_gravity_x);
  EXPECT_TRUE(received_data.has_acceleration_including_gravity_y);
  EXPECT_EQ(2, received_data.acceleration_including_gravity_y);
  EXPECT_TRUE(received_data.has_acceleration_including_gravity_z);
  EXPECT_EQ(3, received_data.acceleration_including_gravity_z);

  EXPECT_FALSE(received_data.has_acceleration_x);
  EXPECT_FALSE(received_data.has_acceleration_y);
  EXPECT_FALSE(received_data.has_acceleration_z);

  EXPECT_TRUE(received_data.has_rotation_rate_alpha);
  EXPECT_EQ(7, received_data.rotation_rate_alpha);
  EXPECT_TRUE(received_data.has_rotation_rate_beta);
  EXPECT_EQ(8, received_data.rotation_rate_beta);
  EXPECT_TRUE(received_data.has_rotation_rate_gamma);
  EXPECT_EQ(9, received_data.rotation_rate_gamma);
}

TEST_F(DeviceMotionEventPumpTest, NoActiveSensors) {
  motion_pump()->Start(listener());
  motion_pump()->StartFireEvent();

  base::RunLoop().Run();

  device::MotionData received_data = listener()->data();
  EXPECT_TRUE(listener()->did_change_device_motion());

  EXPECT_FALSE(received_data.has_acceleration_x);
  EXPECT_FALSE(received_data.has_acceleration_y);
  EXPECT_FALSE(received_data.has_acceleration_z);

  EXPECT_FALSE(received_data.has_acceleration_including_gravity_x);
  EXPECT_FALSE(received_data.has_acceleration_including_gravity_y);
  EXPECT_FALSE(received_data.has_acceleration_including_gravity_z);

  EXPECT_FALSE(received_data.has_rotation_rate_alpha);
  EXPECT_FALSE(received_data.has_rotation_rate_beta);
  EXPECT_FALSE(received_data.has_rotation_rate_gamma);
}

// Confirm that the frequency of pumping events is not greater than 60Hz. A rate
// above 60Hz would allow for the detection of keystrokes (crbug.com/421691)
TEST_F(DeviceMotionEventPumpTest, PumpThrottlesEventRate) {
  // Confirm that the delay for pumping events is 60 Hz.
  EXPECT_GE(60, base::Time::kMicrosecondsPerSecond /
      motion_pump()->pump_delay_microseconds());

  motion_pump()->Start(listener());
  motion_pump()->SetLinearAccelerationSensorData(true /* active */, 4, 5, 6);

  motion_pump()->set_stop_on_fire_event(false);
  motion_pump()->StartFireEvent();

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      base::TimeDelta::FromMilliseconds(100));
  base::RunLoop().Run();
  motion_pump()->Stop();

  // Check that the blink::WebDeviceMotionListener does not receive excess
  // events.
  EXPECT_TRUE(listener()->did_change_device_motion());
  EXPECT_GE(6, listener()->number_of_events());
}

}  // namespace content
