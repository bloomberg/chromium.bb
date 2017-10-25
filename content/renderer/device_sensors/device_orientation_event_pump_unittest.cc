// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/device_sensors/device_orientation_event_pump.h"

#include <string.h>

#include <cmath>

#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/test/test_utils.h"
#include "device/sensors/public/cpp/orientation_data.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading_shared_buffer_reader.h"
#include "services/device/public/interfaces/sensor.mojom.h"
#include "services/device/public/interfaces/sensor_provider.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/modules/device_orientation/WebDeviceOrientationListener.h"

namespace {

constexpr uint64_t kReadingBufferSize =
    sizeof(device::SensorReadingSharedBuffer);

constexpr uint64_t kSharedBufferSizeInBytes =
    kReadingBufferSize * static_cast<uint64_t>(device::mojom::SensorType::LAST);

}  // namespace

namespace content {

class MockDeviceOrientationListener
    : public blink::WebDeviceOrientationListener {
 public:
  MockDeviceOrientationListener() : did_change_device_orientation_(false) {
    memset(&data_, 0, sizeof(data_));
  }
  ~MockDeviceOrientationListener() override {}

  void DidChangeDeviceOrientation(
      const device::OrientationData& data) override {
    memcpy(&data_, &data, sizeof(data));
    did_change_device_orientation_ = true;
  }

  bool did_change_device_orientation() const {
    return did_change_device_orientation_;
  }
  void set_did_change_device_orientation(bool value) {
    did_change_device_orientation_ = value;
  }
  const device::OrientationData& data() const { return data_; }

 private:
  bool did_change_device_orientation_;
  device::OrientationData data_;

  DISALLOW_COPY_AND_ASSIGN(MockDeviceOrientationListener);
};

class DeviceOrientationEventPumpForTesting : public DeviceOrientationEventPump {
 public:
  DeviceOrientationEventPumpForTesting(bool absolute)
      : DeviceOrientationEventPump(nullptr, absolute) {}
  ~DeviceOrientationEventPumpForTesting() override {}

  // DeviceOrientationEventPump:
  void SendStartMessage() override {
    relative_orientation_sensor_.mode =
        device::mojom::ReportingMode::CONTINUOUS;
    absolute_orientation_sensor_.mode = device::mojom::ReportingMode::ON_CHANGE;

    shared_memory_ = mojo::SharedBufferHandle::Create(kSharedBufferSizeInBytes);

    relative_orientation_sensor_.shared_buffer_handle = shared_memory_->Clone();
    relative_orientation_sensor_.shared_buffer = shared_memory_->MapAtOffset(
        kReadingBufferSize, device::SensorReadingSharedBuffer::GetOffset(
                                relative_orientation_sensor_.type));
    relative_orientation_sensor_buffer_ =
        static_cast<device::SensorReadingSharedBuffer*>(
            relative_orientation_sensor_.shared_buffer.get());
    relative_orientation_sensor_.shared_buffer_reader.reset(
        new device::SensorReadingSharedBufferReader(
            relative_orientation_sensor_buffer_));

    absolute_orientation_sensor_.shared_buffer_handle = shared_memory_->Clone();
    absolute_orientation_sensor_.shared_buffer = shared_memory_->MapAtOffset(
        kReadingBufferSize, device::SensorReadingSharedBuffer::GetOffset(
                                absolute_orientation_sensor_.type));
    absolute_orientation_sensor_buffer_ =
        static_cast<device::SensorReadingSharedBuffer*>(
            absolute_orientation_sensor_.shared_buffer.get());
    absolute_orientation_sensor_.shared_buffer_reader.reset(
        new device::SensorReadingSharedBufferReader(
            absolute_orientation_sensor_buffer_));
  }

  void StartFireEvent() { DeviceSensorEventPump::DidStartIfPossible(); }

  void SetRelativeOrientationSensorData(bool active,
                                        double alpha,
                                        double beta,
                                        double gamma) {
    if (active) {
      mojo::MakeRequest(&relative_orientation_sensor_.sensor);
      relative_orientation_sensor_buffer_->reading.orientation_euler.timestamp =
          (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
      relative_orientation_sensor_buffer_->reading.orientation_euler.x = beta;
      relative_orientation_sensor_buffer_->reading.orientation_euler.y = gamma;
      relative_orientation_sensor_buffer_->reading.orientation_euler.z = alpha;
    } else {
      relative_orientation_sensor_.sensor.reset();
    }
  }

  void InitializeRelativeOrientationSensorPtr() {
    mojo::MakeRequest(&relative_orientation_sensor_.sensor);
  }

  void InitializeRelativeOrientationSensorSharedBuffer() {
    shared_memory_ = mojo::SharedBufferHandle::Create(kSharedBufferSizeInBytes);
    relative_orientation_sensor_.shared_buffer = shared_memory_->MapAtOffset(
        kReadingBufferSize, device::SensorReadingSharedBuffer::GetOffset(
                                relative_orientation_sensor_.type));
  }

  void SetAbsoluteOrientationSensorData(bool active,
                                        double alpha,
                                        double beta,
                                        double gamma) {
    if (active) {
      mojo::MakeRequest(&absolute_orientation_sensor_.sensor);
      absolute_orientation_sensor_buffer_->reading.orientation_euler.timestamp =
          (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
      absolute_orientation_sensor_buffer_->reading.orientation_euler.x = beta;
      absolute_orientation_sensor_buffer_->reading.orientation_euler.y = gamma;
      absolute_orientation_sensor_buffer_->reading.orientation_euler.z = alpha;
    } else {
      absolute_orientation_sensor_.sensor.reset();
    }
  }

 protected:
  // DeviceOrientationEventPump:
  void FireEvent() override {
    DeviceOrientationEventPump::FireEvent();
    Stop();
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }

 private:
  mojo::ScopedSharedBufferHandle shared_memory_;
  device::SensorReadingSharedBuffer* relative_orientation_sensor_buffer_;
  device::SensorReadingSharedBuffer* absolute_orientation_sensor_buffer_;

  DISALLOW_COPY_AND_ASSIGN(DeviceOrientationEventPumpForTesting);
};

class DeviceOrientationEventPumpTest : public testing::Test {
 public:
  DeviceOrientationEventPumpTest() = default;

 protected:
  void SetUp() override {
    listener_.reset(new MockDeviceOrientationListener);
    orientation_pump_.reset(
        new DeviceOrientationEventPumpForTesting(false /* absolute */));
    absolute_orientation_pump_.reset(
        new DeviceOrientationEventPumpForTesting(true /* absolute */));
  }

  MockDeviceOrientationListener* listener() { return listener_.get(); }
  DeviceOrientationEventPumpForTesting* orientation_pump() {
    return orientation_pump_.get();
  }

  DeviceOrientationEventPumpForTesting* absolute_orientation_pump() {
    return absolute_orientation_pump_.get();
  }

 private:
  base::MessageLoop loop_;
  std::unique_ptr<MockDeviceOrientationListener> listener_;
  std::unique_ptr<DeviceOrientationEventPumpForTesting> orientation_pump_;
  std::unique_ptr<DeviceOrientationEventPumpForTesting>
      absolute_orientation_pump_;

  DISALLOW_COPY_AND_ASSIGN(DeviceOrientationEventPumpTest);
};

TEST_F(DeviceOrientationEventPumpTest, RelativeOrientationSensorIsActive) {
  orientation_pump()->Start(listener());

  orientation_pump()->SetRelativeOrientationSensorData(
      true /* active */, 1 /* alpha */, 2 /* beta */, 3 /* gamma */);

  orientation_pump()->StartFireEvent();

  base::RunLoop().Run();

  device::OrientationData received_data = listener()->data();
  EXPECT_TRUE(listener()->did_change_device_orientation());

  // DeviceOrientation Event provides relative orientation data when it is
  // available.
  EXPECT_EQ(1, static_cast<double>(received_data.alpha));
  EXPECT_TRUE(received_data.has_alpha);
  EXPECT_EQ(2, static_cast<double>(received_data.beta));
  EXPECT_TRUE(received_data.has_beta);
  EXPECT_EQ(3, static_cast<double>(received_data.gamma));
  EXPECT_TRUE(received_data.has_gamma);
  EXPECT_FALSE(received_data.absolute);
}

TEST_F(DeviceOrientationEventPumpTest, AbsoluteOrientationSensorIsActive) {
  absolute_orientation_pump()->Start(listener());

  absolute_orientation_pump()->SetAbsoluteOrientationSensorData(
      true /* active */, 4 /* alpha */, 5 /* beta */, 6 /* gamma */);
  absolute_orientation_pump()->StartFireEvent();

  base::RunLoop().Run();

  device::OrientationData received_data = listener()->data();
  EXPECT_TRUE(listener()->did_change_device_orientation());

  EXPECT_EQ(4, static_cast<double>(received_data.alpha));
  EXPECT_TRUE(received_data.has_alpha);
  EXPECT_EQ(5, static_cast<double>(received_data.beta));
  EXPECT_TRUE(received_data.has_beta);
  EXPECT_EQ(6, static_cast<double>(received_data.gamma));
  EXPECT_TRUE(received_data.has_gamma);
  EXPECT_TRUE(received_data.absolute);
}

TEST_F(DeviceOrientationEventPumpTest,
       RelativeOrientationSensorSomeDataFieldsNotAvailable) {
  orientation_pump()->Start(listener());

  orientation_pump()->SetRelativeOrientationSensorData(
      true /* active */, NAN /* alpha */, 2 /* beta */, 3 /* gamma */);
  orientation_pump()->StartFireEvent();

  base::RunLoop().Run();

  device::OrientationData received_data = listener()->data();
  EXPECT_TRUE(listener()->did_change_device_orientation());

  EXPECT_FALSE(received_data.has_alpha);
  EXPECT_EQ(2, static_cast<double>(received_data.beta));
  EXPECT_TRUE(received_data.has_beta);
  EXPECT_EQ(3, static_cast<double>(received_data.gamma));
  EXPECT_TRUE(received_data.has_gamma);
  EXPECT_FALSE(received_data.absolute);
}

TEST_F(DeviceOrientationEventPumpTest,
       AbsoluteOrientationSensorSomeDataFieldsNotAvailable) {
  absolute_orientation_pump()->Start(listener());

  absolute_orientation_pump()->SetAbsoluteOrientationSensorData(
      true /* active */, 4 /* alpha */, NAN /* beta */, 6 /* gamma */);
  absolute_orientation_pump()->StartFireEvent();

  base::RunLoop().Run();

  device::OrientationData received_data = listener()->data();
  EXPECT_TRUE(listener()->did_change_device_orientation());

  EXPECT_EQ(4, static_cast<double>(received_data.alpha));
  EXPECT_TRUE(received_data.has_alpha);
  EXPECT_FALSE(received_data.has_beta);
  EXPECT_EQ(6, static_cast<double>(received_data.gamma));
  EXPECT_TRUE(received_data.has_gamma);
  EXPECT_TRUE(received_data.absolute);
}

TEST_F(DeviceOrientationEventPumpTest,
       DeviceOrientationEventFallbackFromRelativeToAbsolute) {
  orientation_pump()->Start(listener());

  orientation_pump()->SetRelativeOrientationSensorData(
      false /* active */, 1 /* alpha */, 2 /* beta */, 3 /* gamma */);
  orientation_pump()->SetAbsoluteOrientationSensorData(
      true /* active */, 4 /* alpha */, 5 /* beta */, 6 /* gamma */);
  orientation_pump()->StartFireEvent();

  base::RunLoop().Run();

  device::OrientationData received_data = listener()->data();
  EXPECT_TRUE(listener()->did_change_device_orientation());

  // DeviceOrientation Event provides absolute orientation data when relative
  // orientation data is not available but absolute orientation data is
  // available.
  EXPECT_EQ(4, static_cast<double>(received_data.alpha));
  EXPECT_TRUE(received_data.has_alpha);
  EXPECT_EQ(5, static_cast<double>(received_data.beta));
  EXPECT_TRUE(received_data.has_beta);
  EXPECT_EQ(6, static_cast<double>(received_data.gamma));
  EXPECT_TRUE(received_data.has_gamma);
  // Since no relative orientation data is available, DeviceOrientationEvent
  // fallback to provide absolute orientation data.
  EXPECT_TRUE(received_data.absolute);
}

TEST_F(DeviceOrientationEventPumpTest, DeviceOrientationFireAllNullEvent) {
  orientation_pump()->Start(listener());

  orientation_pump()->StartFireEvent();

  base::RunLoop().Run();

  device::OrientationData received_data = listener()->data();
  EXPECT_TRUE(listener()->did_change_device_orientation());

  EXPECT_FALSE(received_data.has_alpha);
  EXPECT_FALSE(received_data.has_beta);
  EXPECT_FALSE(received_data.has_gamma);
  EXPECT_FALSE(received_data.absolute);
}

TEST_F(DeviceOrientationEventPumpTest,
       DeviceOrientationAbsoluteFireAllNullEvent) {
  absolute_orientation_pump()->Start(listener());

  absolute_orientation_pump()->StartFireEvent();

  base::RunLoop().Run();

  device::OrientationData received_data = listener()->data();
  EXPECT_TRUE(listener()->did_change_device_orientation());

  EXPECT_FALSE(received_data.has_alpha);
  EXPECT_FALSE(received_data.has_beta);
  EXPECT_FALSE(received_data.has_gamma);
  EXPECT_TRUE(received_data.absolute);
}

TEST_F(DeviceOrientationEventPumpTest, UpdateRespectsOrientationThreshold) {
  orientation_pump()->Start(listener());

  orientation_pump()->SetRelativeOrientationSensorData(
      true /* active */, 1 /* alpha */, 2 /* beta */, 3 /* gamma */);

  orientation_pump()->StartFireEvent();

  base::RunLoop().Run();

  device::OrientationData received_data = listener()->data();
  EXPECT_TRUE(listener()->did_change_device_orientation());

  EXPECT_EQ(1, static_cast<double>(received_data.alpha));
  EXPECT_TRUE(received_data.has_alpha);
  EXPECT_EQ(2, static_cast<double>(received_data.beta));
  EXPECT_TRUE(received_data.has_beta);
  EXPECT_EQ(3, static_cast<double>(received_data.gamma));
  EXPECT_TRUE(received_data.has_gamma);
  EXPECT_FALSE(received_data.absolute);

  // Reset the pump's listener.
  orientation_pump()->Start(listener());

  orientation_pump()->SetRelativeOrientationSensorData(
      true /* active */,
      1 + DeviceOrientationEventPump::kOrientationThreshold / 2.0 /* alpha */,
      2 /* beta */, 3 /* gamma */);
  listener()->set_did_change_device_orientation(false);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&DeviceOrientationEventPumpForTesting::StartFireEvent,
                     base::Unretained(orientation_pump())));

  base::RunLoop().Run();

  received_data = listener()->data();
  EXPECT_FALSE(listener()->did_change_device_orientation());

  EXPECT_EQ(1, static_cast<double>(received_data.alpha));
  EXPECT_TRUE(received_data.has_alpha);
  EXPECT_EQ(2, static_cast<double>(received_data.beta));
  EXPECT_TRUE(received_data.has_beta);
  EXPECT_EQ(3, static_cast<double>(received_data.gamma));
  EXPECT_TRUE(received_data.has_gamma);
  EXPECT_FALSE(received_data.absolute);

  // Reset the pump's listener.
  orientation_pump()->Start(listener());

  orientation_pump()->SetRelativeOrientationSensorData(
      true /* active */,
      1 + DeviceOrientationEventPump::kOrientationThreshold /* alpha */,
      2 /* beta */, 3 /* gamma */);
  listener()->set_did_change_device_orientation(false);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&DeviceOrientationEventPumpForTesting::StartFireEvent,
                     base::Unretained(orientation_pump())));

  base::RunLoop().Run();

  received_data = listener()->data();
  EXPECT_TRUE(listener()->did_change_device_orientation());
  EXPECT_EQ(1 + DeviceOrientationEventPump::kOrientationThreshold,
            static_cast<double>(received_data.alpha));
}

TEST_F(DeviceOrientationEventPumpTest,
       SensorInitializedButItsSharedBufferIsNot) {
  // Initialize the |sensor|, but do not initialize its |shared_buffer|, this
  // is to test the state when |sensor| is already initialized but its
  // |shared_buffer| is not initialized yet, and make sure that the
  // DeviceOrientationEventPump can not be started until |shared_buffer| is
  // initialized.
  EXPECT_FALSE(orientation_pump()->relative_orientation_sensor_.sensor);
  orientation_pump()->InitializeRelativeOrientationSensorPtr();
  EXPECT_TRUE(orientation_pump()->relative_orientation_sensor_.sensor);
  EXPECT_FALSE(orientation_pump()->relative_orientation_sensor_.shared_buffer);
  EXPECT_FALSE(orientation_pump()->SensorSharedBuffersReady());

  // Initialize |shared_buffer| and make sure that now
  // DeviceOrientationEventPump can be started.
  orientation_pump()->InitializeRelativeOrientationSensorSharedBuffer();
  EXPECT_TRUE(orientation_pump()->relative_orientation_sensor_.shared_buffer);
  EXPECT_TRUE(orientation_pump()->SensorSharedBuffersReady());
}

}  // namespace content
