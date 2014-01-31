// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_motion_event_pump.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "content/common/device_orientation/device_motion_hardware_buffer.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebDeviceMotionListener.h"

namespace content {

class MockDeviceMotionListener : public blink::WebDeviceMotionListener {
 public:
  MockDeviceMotionListener();
  virtual ~MockDeviceMotionListener() { }
  virtual void didChangeDeviceMotion(
      const blink::WebDeviceMotionData&) OVERRIDE;
  bool did_change_device_motion_;
  blink::WebDeviceMotionData data_;
};

MockDeviceMotionListener::MockDeviceMotionListener()
    : did_change_device_motion_(false) {
  memset(&data_, 0, sizeof(data_));
}

void MockDeviceMotionListener::didChangeDeviceMotion(
    const blink::WebDeviceMotionData& data) {
  memcpy(&data_, &data, sizeof(data));
  did_change_device_motion_ = true;
}

class DeviceMotionEventPumpForTesting : public DeviceMotionEventPump {
 public:
  DeviceMotionEventPumpForTesting() { }
  virtual ~DeviceMotionEventPumpForTesting() { }

  void OnDidStart(base::SharedMemoryHandle renderer_handle) {
    DeviceMotionEventPump::OnDidStart(renderer_handle);
  }
  virtual bool SendStartMessage() OVERRIDE { return true; }
  virtual bool SendStopMessage() OVERRIDE { return true; }
  virtual void FireEvent() OVERRIDE {
    DeviceMotionEventPump::FireEvent();
    Stop();
    base::MessageLoop::current()->QuitWhenIdle();
  }
};

class DeviceMotionEventPumpTest : public testing::Test {
 public:
  DeviceMotionEventPumpTest() {
    EXPECT_TRUE(shared_memory_.CreateAndMapAnonymous(
        sizeof(DeviceMotionHardwareBuffer)));
  }

 protected:
  virtual void SetUp() OVERRIDE {
    const DeviceMotionHardwareBuffer* null_buffer = NULL;
    listener_.reset(new MockDeviceMotionListener);
    motion_pump_.reset(new DeviceMotionEventPumpForTesting);
    buffer_ = static_cast<DeviceMotionHardwareBuffer*>(shared_memory_.memory());
    ASSERT_NE(null_buffer, buffer_);
    memset(buffer_, 0, sizeof(DeviceMotionHardwareBuffer));
    ASSERT_TRUE(shared_memory_.ShareToProcess(base::GetCurrentProcessHandle(),
        &handle_));
  }

  void InitBuffer(bool allAvailableSensorsActive) {
    blink::WebDeviceMotionData& data = buffer_->data;
    data.accelerationX = 1;
    data.hasAccelerationX = true;
    data.accelerationY = 2;
    data.hasAccelerationY = true;
    data.accelerationZ = 3;
    data.hasAccelerationZ = true;
    data.allAvailableSensorsAreActive = allAvailableSensorsActive;
  }

  scoped_ptr<MockDeviceMotionListener> listener_;
  scoped_ptr<DeviceMotionEventPumpForTesting> motion_pump_;
  base::SharedMemoryHandle handle_;
  base::SharedMemory shared_memory_;
  DeviceMotionHardwareBuffer* buffer_;
};

TEST_F(DeviceMotionEventPumpTest, DidStartPolling) {
  base::MessageLoopForUI loop;

  InitBuffer(true);

  motion_pump_->SetListener(listener_.get());
  motion_pump_->OnDidStart(handle_);

  base::MessageLoop::current()->Run();

  blink::WebDeviceMotionData& received_data = listener_->data_;
  EXPECT_TRUE(listener_->did_change_device_motion_);
  EXPECT_TRUE(received_data.hasAccelerationX);
  EXPECT_EQ(1, (double)received_data.accelerationX);
  EXPECT_TRUE(received_data.hasAccelerationX);
  EXPECT_EQ(2, (double)received_data.accelerationY);
  EXPECT_TRUE(received_data.hasAccelerationY);
  EXPECT_EQ(3, (double)received_data.accelerationZ);
  EXPECT_TRUE(received_data.hasAccelerationZ);
  EXPECT_FALSE(received_data.hasAccelerationIncludingGravityX);
  EXPECT_FALSE(received_data.hasAccelerationIncludingGravityY);
  EXPECT_FALSE(received_data.hasAccelerationIncludingGravityZ);
  EXPECT_FALSE(received_data.hasRotationRateAlpha);
  EXPECT_FALSE(received_data.hasRotationRateBeta);
  EXPECT_FALSE(received_data.hasRotationRateGamma);
}

TEST_F(DeviceMotionEventPumpTest, DidStartPollingNotAllSensorsActive) {
  base::MessageLoopForUI loop;

  InitBuffer(false);

  motion_pump_->SetListener(listener_.get());
  motion_pump_->OnDidStart(handle_);

  base::MessageLoop::current()->Run();

  blink::WebDeviceMotionData& received_data = listener_->data_;
  // No change in device motion because allAvailableSensorsAreActive is false.
  EXPECT_FALSE(listener_->did_change_device_motion_);
  EXPECT_FALSE(received_data.hasAccelerationX);
  EXPECT_FALSE(received_data.hasAccelerationX);
  EXPECT_FALSE(received_data.hasAccelerationY);
  EXPECT_FALSE(received_data.hasAccelerationZ);
  EXPECT_FALSE(received_data.hasAccelerationIncludingGravityX);
  EXPECT_FALSE(received_data.hasAccelerationIncludingGravityY);
  EXPECT_FALSE(received_data.hasAccelerationIncludingGravityZ);
  EXPECT_FALSE(received_data.hasRotationRateAlpha);
  EXPECT_FALSE(received_data.hasRotationRateBeta);
  EXPECT_FALSE(received_data.hasRotationRateGamma);
}

}  // namespace content
