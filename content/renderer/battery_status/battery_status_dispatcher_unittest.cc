// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "battery_status_dispatcher.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/battery_status_messages.h"
#include "content/public/test/mock_render_thread.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebBatteryStatusListener.h"

namespace content {

class MockBatteryStatusListener : public blink::WebBatteryStatusListener {
 public:
  MockBatteryStatusListener() : did_change_battery_status_(false) { }
  virtual ~MockBatteryStatusListener() { }

  // blink::WebBatteryStatusListener method.
  virtual void updateBatteryStatus(
      const blink::WebBatteryStatus& status) OVERRIDE {
    status_ = status;
    did_change_battery_status_ = true;
  }

  const blink::WebBatteryStatus& status() const { return status_; }
  bool did_change_battery_status() const { return did_change_battery_status_; }

 private:
  bool did_change_battery_status_;
  blink::WebBatteryStatus status_;

  DISALLOW_COPY_AND_ASSIGN(MockBatteryStatusListener);
};

class BatteryStatusDispatcherTest : public testing::Test {
 private:
  // We need to create a MockRenderThread so RenderThread::Get() doesn't return
  // null.
  MockRenderThread render_thread_;
};

TEST_F(BatteryStatusDispatcherTest, UpdateListener) {
  MockBatteryStatusListener listener;
  BatteryStatusDispatcher dispatcher(0);

  blink::WebBatteryStatus status;
  status.charging = true;
  status.chargingTime = 100;
  status.dischargingTime = 200;
  status.level = 0.5;

  dispatcher.Start(&listener);

  BatteryStatusMsg_DidChange message(status);
  dispatcher.OnControlMessageReceived(message);

  const blink::WebBatteryStatus& received_status = listener.status();
  EXPECT_TRUE(listener.did_change_battery_status());
  EXPECT_EQ(status.charging, received_status.charging);
  EXPECT_EQ(status.chargingTime, received_status.chargingTime);
  EXPECT_EQ(status.dischargingTime, received_status.dischargingTime);
  EXPECT_EQ(status.level, received_status.level);

  dispatcher.Stop();
}

TEST_F(BatteryStatusDispatcherTest, NoUpdateWhenNullListener) {
  MockBatteryStatusListener listener;
  BatteryStatusDispatcher dispatcher(0);

  dispatcher.Start(0);
  dispatcher.Stop();

  blink::WebBatteryStatus status;
  BatteryStatusMsg_DidChange message(status);
  dispatcher.OnControlMessageReceived(message);
  EXPECT_FALSE(listener.did_change_battery_status());
}

}  // namespace content
