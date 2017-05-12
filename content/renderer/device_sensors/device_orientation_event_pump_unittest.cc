// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_orientation_event_pump.h"

#include <string.h>

#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/test/test_utils.h"
#include "device/sensors/public/cpp/device_orientation_hardware_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/modules/device_orientation/WebDeviceOrientationListener.h"

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
  DeviceOrientationEventPumpForTesting()
      : DeviceOrientationEventPump(nullptr) {}
  ~DeviceOrientationEventPumpForTesting() override {}

  void DidStart(mojo::ScopedSharedBufferHandle renderer_handle) {
    DeviceOrientationEventPump::DidStart(std::move(renderer_handle));
  }
  void SendStartMessage() override {}
  void SendStopMessage() override {}
  void FireEvent() override {
    DeviceOrientationEventPump::FireEvent();
    Stop();
    base::MessageLoop::current()->QuitWhenIdle();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceOrientationEventPumpForTesting);
};

class DeviceOrientationEventPumpTest : public testing::Test {
 public:
  DeviceOrientationEventPumpTest() = default;

 protected:
  void SetUp() override {
    listener_.reset(new MockDeviceOrientationListener);
    orientation_pump_.reset(new DeviceOrientationEventPumpForTesting);
    shared_memory_ = mojo::SharedBufferHandle::Create(
        sizeof(device::DeviceOrientationHardwareBuffer));
    mapping_ =
        shared_memory_->Map(sizeof(device::DeviceOrientationHardwareBuffer));
    ASSERT_TRUE(mapping_);
    memset(buffer(), 0, sizeof(device::DeviceOrientationHardwareBuffer));
  }

  void InitBuffer() {
    device::OrientationData& data = buffer()->data;
    data.alpha = 1;
    data.has_alpha = true;
    data.beta = 2;
    data.has_beta = true;
    data.gamma = 3;
    data.has_gamma = true;
    data.all_available_sensors_are_active = true;
  }

  void InitBufferNoData() {
    device::OrientationData& data = buffer()->data;
    data.all_available_sensors_are_active = true;
  }

  MockDeviceOrientationListener* listener() { return listener_.get(); }
  DeviceOrientationEventPumpForTesting* orientation_pump() {
    return orientation_pump_.get();
  }
  mojo::ScopedSharedBufferHandle handle() {
    return shared_memory_->Clone(
        mojo::SharedBufferHandle::AccessMode::READ_ONLY);
  }
  device::DeviceOrientationHardwareBuffer* buffer() {
    return reinterpret_cast<device::DeviceOrientationHardwareBuffer*>(
        mapping_.get());
  }

 private:
  base::MessageLoop loop_;
  std::unique_ptr<MockDeviceOrientationListener> listener_;
  std::unique_ptr<DeviceOrientationEventPumpForTesting> orientation_pump_;
  mojo::ScopedSharedBufferHandle shared_memory_;
  mojo::ScopedSharedBufferMapping mapping_;

  DISALLOW_COPY_AND_ASSIGN(DeviceOrientationEventPumpTest);
};

TEST_F(DeviceOrientationEventPumpTest, DidStartPolling) {
  InitBuffer();
  orientation_pump()->Start(listener());
  orientation_pump()->DidStart(handle());

  base::RunLoop().Run();

  const device::OrientationData& received_data = listener()->data();
  EXPECT_TRUE(listener()->did_change_device_orientation());
  EXPECT_TRUE(received_data.all_available_sensors_are_active);
  EXPECT_EQ(1, static_cast<double>(received_data.alpha));
  EXPECT_TRUE(received_data.has_alpha);
  EXPECT_EQ(2, static_cast<double>(received_data.beta));
  EXPECT_TRUE(received_data.has_beta);
  EXPECT_EQ(3, static_cast<double>(received_data.gamma));
  EXPECT_TRUE(received_data.has_gamma);
}

TEST_F(DeviceOrientationEventPumpTest, FireAllNullEvent) {
  InitBufferNoData();
  orientation_pump()->Start(listener());
  orientation_pump()->DidStart(handle());

  base::RunLoop().Run();

  const device::OrientationData& received_data = listener()->data();
  EXPECT_TRUE(listener()->did_change_device_orientation());
  EXPECT_TRUE(received_data.all_available_sensors_are_active);
  EXPECT_FALSE(received_data.has_alpha);
  EXPECT_FALSE(received_data.has_beta);
  EXPECT_FALSE(received_data.has_gamma);
}

TEST_F(DeviceOrientationEventPumpTest, UpdateRespectsOrientationThreshold) {
  InitBuffer();
  orientation_pump()->Start(listener());
  orientation_pump()->DidStart(handle());

  base::RunLoop().Run();

  const device::OrientationData& received_data = listener()->data();
  EXPECT_TRUE(listener()->did_change_device_orientation());
  EXPECT_TRUE(received_data.all_available_sensors_are_active);
  EXPECT_EQ(1, static_cast<double>(received_data.alpha));
  EXPECT_TRUE(received_data.has_alpha);
  EXPECT_EQ(2, static_cast<double>(received_data.beta));
  EXPECT_TRUE(received_data.has_beta);
  EXPECT_EQ(3, static_cast<double>(received_data.gamma));
  EXPECT_TRUE(received_data.has_gamma);

  buffer()->data.alpha =
      1 + DeviceOrientationEventPump::kOrientationThreshold / 2.0;
  listener()->set_did_change_device_orientation(false);

  // Reset the pump's listener.
  orientation_pump()->Start(listener());

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&DeviceOrientationEventPumpForTesting::FireEvent,
                            base::Unretained(orientation_pump())));
  base::RunLoop().Run();

  EXPECT_FALSE(listener()->did_change_device_orientation());
  EXPECT_TRUE(received_data.all_available_sensors_are_active);
  EXPECT_EQ(1, static_cast<double>(received_data.alpha));
  EXPECT_TRUE(received_data.has_alpha);
  EXPECT_EQ(2, static_cast<double>(received_data.beta));
  EXPECT_TRUE(received_data.has_beta);
  EXPECT_EQ(3, static_cast<double>(received_data.gamma));
  EXPECT_TRUE(received_data.has_gamma);

  buffer()->data.alpha =
      1 + DeviceOrientationEventPump::kOrientationThreshold;
  listener()->set_did_change_device_orientation(false);

  // Reset the pump's listener.
  orientation_pump()->Start(listener());

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&DeviceOrientationEventPumpForTesting::FireEvent,
                            base::Unretained(orientation_pump())));
  base::RunLoop().Run();

  EXPECT_TRUE(listener()->did_change_device_orientation());
  EXPECT_EQ(1 + DeviceOrientationEventPump::kOrientationThreshold,
      static_cast<double>(received_data.alpha));
}

}  // namespace content
