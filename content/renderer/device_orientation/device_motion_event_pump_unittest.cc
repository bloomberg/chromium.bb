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

class DeviceMotionEventPumpTest : public testing::Test {
};

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
};

// Always failing in the win try bot. See http://crbug.com/256782.
#if defined(OS_WIN)
#define MAYBE_DidStartPolling DISABLED_DidStartPolling
#else
#define MAYBE_DidStartPolling DidStartPolling
#endif
TEST_F(DeviceMotionEventPumpTest, MAYBE_DidStartPolling) {
  base::MessageLoop loop(base::MessageLoop::TYPE_UI);
  scoped_ptr<MockDeviceMotionListener> listener(new MockDeviceMotionListener);
  scoped_ptr<DeviceMotionEventPumpForTesting> motion_pump(
      new DeviceMotionEventPumpForTesting);

  base::SharedMemoryHandle handle;
  base::SharedMemory shared_memory;
  EXPECT_TRUE(shared_memory.CreateAndMapAnonymous(
      sizeof(DeviceMotionHardwareBuffer)));
  DeviceMotionHardwareBuffer* buffer =
      static_cast<DeviceMotionHardwareBuffer*>(shared_memory.memory());
  memset(buffer, 0, sizeof(DeviceMotionHardwareBuffer));
  shared_memory.ShareToProcess(base::kNullProcessHandle, &handle);

  blink::WebDeviceMotionData& data = buffer->data;
  data.accelerationX = 1;
  data.hasAccelerationX = true;
  data.accelerationY = 2;
  data.hasAccelerationY = true;
  data.accelerationZ = 3;
  data.hasAccelerationZ = true;
  data.allAvailableSensorsAreActive = true;

  motion_pump->SetListener(listener.get());
  motion_pump->OnDidStart(handle);
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(
      motion_pump->GetDelayMillis() * 2));
  RunAllPendingInMessageLoop();
  motion_pump->SetListener(0);

  blink::WebDeviceMotionData& received_data = listener->data_;
  EXPECT_TRUE(listener->did_change_device_motion_);
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

// Although this test passes on windows builds it is not certain if it does
// so for the right reason. See http://crbug.com/256782.
#if defined(OS_WIN)
#define MAYBE_DidStartPollingNotAllSensorsActive \
    DISABLED_DidStartPollingNotAllSensorsActive
#else
#define MAYBE_DidStartPollingNotAllSensorsActive \
    DidStartPollingNotAllSensorsActive
#endif
TEST_F(DeviceMotionEventPumpTest, MAYBE_DidStartPollingNotAllSensorsActive) {
  base::MessageLoop loop(base::MessageLoop::TYPE_UI);
  scoped_ptr<MockDeviceMotionListener> listener(new MockDeviceMotionListener);
  scoped_ptr<DeviceMotionEventPumpForTesting> motion_pump(
      new DeviceMotionEventPumpForTesting);

  base::SharedMemoryHandle handle;
  base::SharedMemory shared_memory;
  EXPECT_TRUE(shared_memory.CreateAndMapAnonymous(
      sizeof(DeviceMotionHardwareBuffer)));
  DeviceMotionHardwareBuffer* buffer =
      static_cast<DeviceMotionHardwareBuffer*>(shared_memory.memory());
  memset(buffer, 0, sizeof(DeviceMotionHardwareBuffer));
  shared_memory.ShareToProcess(base::kNullProcessHandle, &handle);

  blink::WebDeviceMotionData& data = buffer->data;
  data.accelerationX = 1;
  data.hasAccelerationX = true;
  data.accelerationY = 2;
  data.hasAccelerationY = true;
  data.accelerationZ = 3;
  data.hasAccelerationZ = true;
  data.allAvailableSensorsAreActive = false;

  motion_pump->SetListener(listener.get());
  motion_pump->OnDidStart(handle);
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(
      motion_pump->GetDelayMillis() * 2));
  RunAllPendingInMessageLoop();
  motion_pump->SetListener(0);

  blink::WebDeviceMotionData& received_data = listener->data_;
  // No change in device motion because allAvailableSensorsAreActive is false.
  EXPECT_FALSE(listener->did_change_device_motion_);
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
