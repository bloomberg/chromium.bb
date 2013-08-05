// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_orientation_event_pump.h"

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "content/common/device_orientation/device_orientation_hardware_buffer.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebDeviceOrientationListener.h"

namespace content {

class MockDeviceOrientationListener
    : public WebKit::WebDeviceOrientationListener {
 public:
  MockDeviceOrientationListener();
  virtual ~MockDeviceOrientationListener() { }
  virtual void didChangeDeviceOrientation(
      const WebKit::WebDeviceOrientationData&) OVERRIDE;
  void ResetDidChangeOrientation();
  bool did_change_device_orientation_;
  WebKit::WebDeviceOrientationData data_;
};

MockDeviceOrientationListener::MockDeviceOrientationListener()
    : did_change_device_orientation_(false) {
  memset(&data_, 0, sizeof(data_));
}

void MockDeviceOrientationListener::didChangeDeviceOrientation(
    const WebKit::WebDeviceOrientationData& data) {
  memcpy(&data_, &data, sizeof(data));
  did_change_device_orientation_ = true;
}

void MockDeviceOrientationListener::ResetDidChangeOrientation() {
  did_change_device_orientation_ = false;
}

class DeviceOrientationEventPumpForTesting : public DeviceOrientationEventPump {
 public:
  DeviceOrientationEventPumpForTesting() { }
  virtual ~DeviceOrientationEventPumpForTesting() { }

  void OnDidStart(base::SharedMemoryHandle renderer_handle) {
    DeviceOrientationEventPump::OnDidStart(renderer_handle);
  }
  virtual bool SendStartMessage() OVERRIDE { return true; }
  virtual bool SendStopMessage() OVERRIDE { return true; }
};

class DeviceOrientationEventPumpTest : public testing::Test {
 public:
  DeviceOrientationEventPumpTest() {
      EXPECT_TRUE(shared_memory_.CreateAndMapAnonymous(
          sizeof(DeviceOrientationHardwareBuffer)));
  }

 protected:
  virtual void SetUp() OVERRIDE {
    listener_.reset(new MockDeviceOrientationListener);
    orientation_pump_.reset(new DeviceOrientationEventPumpForTesting);
    buffer_ = static_cast<DeviceOrientationHardwareBuffer*>(
        shared_memory_.memory());
    memset(buffer_, 0, sizeof(DeviceOrientationHardwareBuffer));
    shared_memory_.ShareToProcess(base::kNullProcessHandle, &handle_);
  }

  void InitBuffer() {
    WebKit::WebDeviceOrientationData& data = buffer_->data;
    data.alpha = 1;
    data.hasAlpha = true;
    data.beta = 2;
    data.hasBeta = true;
    data.gamma = 3;
    data.hasGamma = true;
    data.allAvailableSensorsAreActive = true;
  }

  scoped_ptr<MockDeviceOrientationListener> listener_;
  scoped_ptr<DeviceOrientationEventPumpForTesting> orientation_pump_;
  base::SharedMemoryHandle handle_;
  base::SharedMemory shared_memory_;
  DeviceOrientationHardwareBuffer* buffer_;
};

// Always failing in the win try bot. See http://crbug.com/256782.
#if defined(OS_WIN)
#define MAYBE_DidStartPolling DISABLED_DidStartPolling
#else
#define MAYBE_DidStartPolling DidStartPolling
#endif
TEST_F(DeviceOrientationEventPumpTest, MAYBE_DidStartPolling) {
  base::MessageLoop loop(base::MessageLoop::TYPE_UI);
  InitBuffer();

  orientation_pump_->SetListener(listener_.get());
  orientation_pump_->OnDidStart(handle_);
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(
          orientation_pump_->GetDelayMillis() * 2));
  RunAllPendingInMessageLoop();
  orientation_pump_->SetListener(0);

  WebKit::WebDeviceOrientationData& received_data = listener_->data_;
  EXPECT_TRUE(listener_->did_change_device_orientation_);
  EXPECT_TRUE(received_data.allAvailableSensorsAreActive);
  EXPECT_EQ(1, (double)received_data.alpha);
  EXPECT_TRUE(received_data.hasAlpha);
  EXPECT_EQ(2, (double)received_data.beta);
  EXPECT_TRUE(received_data.hasBeta);
  EXPECT_EQ(3, (double)received_data.gamma);
  EXPECT_TRUE(received_data.hasGamma);
}

// Always failing in the win try bot. See http://crbug.com/256782.
#if defined(OS_WIN)
#define MAYBE_UpdateRespectsOrientationThreshold \
    DISABLED_UpdateRespectsOrientationThreshold
#else
#define MAYBE_UpdateRespectsOrientationThreshold \
    UpdateRespectsOrientationThreshold
#endif
TEST_F(DeviceOrientationEventPumpTest,
    MAYBE_UpdateRespectsOrientationThreshold) {
  base::MessageLoop loop(base::MessageLoop::TYPE_UI);
  InitBuffer();

  orientation_pump_->SetListener(listener_.get());
  orientation_pump_->OnDidStart(handle_);
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(
      orientation_pump_->GetDelayMillis() * 2));
  RunAllPendingInMessageLoop();

  WebKit::WebDeviceOrientationData& received_data = listener_->data_;
  EXPECT_TRUE(listener_->did_change_device_orientation_);
  EXPECT_TRUE(received_data.allAvailableSensorsAreActive);
  EXPECT_EQ(1, (double)received_data.alpha);
  EXPECT_TRUE(received_data.hasAlpha);
  EXPECT_EQ(2, (double)received_data.beta);
  EXPECT_TRUE(received_data.hasBeta);
  EXPECT_EQ(3, (double)received_data.gamma);
  EXPECT_TRUE(received_data.hasGamma);

  buffer_->data.alpha =
      1 + DeviceOrientationEventPump::kOrientationThreshold / 2.0;
  listener_->ResetDidChangeOrientation();

  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(
      orientation_pump_->GetDelayMillis() * 2));
  RunAllPendingInMessageLoop();

  EXPECT_FALSE(listener_->did_change_device_orientation_);
  EXPECT_TRUE(received_data.allAvailableSensorsAreActive);
  EXPECT_EQ(1, (double)received_data.alpha);
  EXPECT_TRUE(received_data.hasAlpha);
  EXPECT_EQ(2, (double)received_data.beta);
  EXPECT_TRUE(received_data.hasBeta);
  EXPECT_EQ(3, (double)received_data.gamma);
  EXPECT_TRUE(received_data.hasGamma);

  buffer_->data.alpha =
      1 + DeviceOrientationEventPump::kOrientationThreshold;
  listener_->ResetDidChangeOrientation();

  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(
      orientation_pump_->GetDelayMillis() * 2));
  RunAllPendingInMessageLoop();
  orientation_pump_->SetListener(0);

  EXPECT_TRUE(listener_->did_change_device_orientation_);
  EXPECT_EQ(1 + DeviceOrientationEventPump::kOrientationThreshold,
      (double)received_data.alpha);
}

}  // namespace content
