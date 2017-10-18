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
#include "device/sensors/public/cpp/motion_data.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/system/buffer.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading_shared_buffer_reader.h"
#include "services/device/public/interfaces/sensor.mojom.h"
#include "services/device/public/interfaces/sensor_provider.mojom.h"
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

    accelerometer_.shared_buffer_handle = shared_memory_->Clone();
    accelerometer_.shared_buffer = shared_memory_->MapAtOffset(
        kReadingBufferSize,
        device::SensorReadingSharedBuffer::GetOffset(accelerometer_.type));
    accelerometer_buffer_ = static_cast<device::SensorReadingSharedBuffer*>(
        accelerometer_.shared_buffer.get());
    accelerometer_.shared_buffer_reader.reset(
        new device::SensorReadingSharedBufferReader(accelerometer_buffer_));

    linear_acceleration_sensor_.shared_buffer_handle = shared_memory_->Clone();
    linear_acceleration_sensor_.shared_buffer = shared_memory_->MapAtOffset(
        kReadingBufferSize, device::SensorReadingSharedBuffer::GetOffset(
                                linear_acceleration_sensor_.type));
    linear_acceleration_sensor_buffer_ =
        static_cast<device::SensorReadingSharedBuffer*>(
            linear_acceleration_sensor_.shared_buffer.get());
    linear_acceleration_sensor_.shared_buffer_reader.reset(
        new device::SensorReadingSharedBufferReader(
            linear_acceleration_sensor_buffer_));

    gyroscope_.shared_buffer_handle = shared_memory_->Clone();
    gyroscope_.shared_buffer = shared_memory_->MapAtOffset(
        kReadingBufferSize,
        device::SensorReadingSharedBuffer::GetOffset(gyroscope_.type));
    gyroscope_buffer_ = static_cast<device::SensorReadingSharedBuffer*>(
        gyroscope_.shared_buffer.get());
    gyroscope_.shared_buffer_reader.reset(
        new device::SensorReadingSharedBufferReader(gyroscope_buffer_));
  }

  void StartFireEvent() { DeviceMotionEventPump::DidStartIfPossible(); }

  void SetAccelerometerSensorData(bool active, double x, double y, double z) {
    if (active) {
      mojo::MakeRequest(&accelerometer_.sensor);
      accelerometer_buffer_->reading.accel.timestamp =
          (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
      accelerometer_buffer_->reading.accel.x = x;
      accelerometer_buffer_->reading.accel.y = y;
      accelerometer_buffer_->reading.accel.z = z;
    } else {
      accelerometer_.sensor.reset();
    }
  }

  void InitializeAccelerometerSensorPtr() {
    mojo::MakeRequest(&accelerometer_.sensor);
  }

  void InitializeAccelerometerSharedBuffer() {
    shared_memory_ = mojo::SharedBufferHandle::Create(kSharedBufferSizeInBytes);
    accelerometer_.shared_buffer = shared_memory_->MapAtOffset(
        kReadingBufferSize,
        device::SensorReadingSharedBuffer::GetOffset(accelerometer_.type));
  }

  void SetLinearAccelerationSensorData(bool active,
                                       double x,
                                       double y,
                                       double z) {
    if (active) {
      mojo::MakeRequest(&linear_acceleration_sensor_.sensor);
      linear_acceleration_sensor_buffer_->reading.accel.timestamp =
          (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
      linear_acceleration_sensor_buffer_->reading.accel.x = x;
      linear_acceleration_sensor_buffer_->reading.accel.y = y;
      linear_acceleration_sensor_buffer_->reading.accel.z = z;
    } else {
      linear_acceleration_sensor_.sensor.reset();
    }
  }

  void SetGyroscopeSensorData(bool active, double x, double y, double z) {
    if (active) {
      mojo::MakeRequest(&gyroscope_.sensor);
      gyroscope_buffer_->reading.gyro.timestamp =
          (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
      gyroscope_buffer_->reading.gyro.x = x;
      gyroscope_buffer_->reading.gyro.y = y;
      gyroscope_buffer_->reading.gyro.z = z;
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
      base::RunLoop::QuitCurrentWhenIdleDeprecated();
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

TEST_F(DeviceMotionEventPumpTest, SensorInitializedButItsSharedBufferIsNot) {
  // Initialize the |sensor|, but do not initialize its |shared_buffer|, this
  // is to test the state when |sensor| is already initialized but its
  // |shared_buffer| is not initialized yet, and make sure that the
  // DeviceMotionEventPump can not be started until |shared_buffer| is
  // initialized.
  EXPECT_FALSE(motion_pump()->accelerometer_.sensor);
  motion_pump()->InitializeAccelerometerSensorPtr();
  EXPECT_TRUE(motion_pump()->accelerometer_.sensor);
  EXPECT_FALSE(motion_pump()->accelerometer_.shared_buffer);
  EXPECT_FALSE(motion_pump()->SensorSharedBuffersReady());

  // Initialize |shared_buffer| and make sure that now DeviceMotionEventPump
  // can be started.
  motion_pump()->InitializeAccelerometerSharedBuffer();
  EXPECT_TRUE(motion_pump()->accelerometer_.shared_buffer);
  EXPECT_TRUE(motion_pump()->SensorSharedBuffersReady());
}

}  // namespace content
