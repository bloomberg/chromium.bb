// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/battery_status/battery_status_dispatcher.h"

#include <utility>

#include "base/macros.h"
#include "content/public/test/mock_render_thread.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebBatteryStatusListener.h"

namespace content {

class MockBatteryStatusListener : public blink::WebBatteryStatusListener {
 public:
  MockBatteryStatusListener() : did_change_battery_status_(false) {}
  ~MockBatteryStatusListener() override {}

  // blink::WebBatteryStatusListener method.
  void updateBatteryStatus(const blink::WebBatteryStatus& status) override {
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
 public:
  void UpdateBatteryStatus(const device::BatteryStatus& status) {
    device::BatteryStatusPtr status_ptr(device::BatteryStatus::New());
    *status_ptr = status;
    dispatcher_->DidChange(std::move(status_ptr));
  }

  const MockBatteryStatusListener& listener() const {
    return listener_;
  }

 protected:
  void SetUp() override {
    dispatcher_.reset(new BatteryStatusDispatcher(&listener_));
  }

 private:
  // We need to create a MockRenderThread so RenderThread::Get() doesn't return
  // null.
  MockRenderThread render_thread_;
  MockBatteryStatusListener listener_;
  scoped_ptr<BatteryStatusDispatcher> dispatcher_;
};

TEST_F(BatteryStatusDispatcherTest, UpdateListener) {
  // TODO(darin): This test isn't super interesting. It just exercises
  // conversion b/w device::BatteryStatus and blink::WebBatteryStatus.

  device::BatteryStatus status;
  status.charging = true;
  status.charging_time = 100;
  status.discharging_time = 200;
  status.level = 0.5;

  UpdateBatteryStatus(status);

  const blink::WebBatteryStatus& received_status = listener().status();
  EXPECT_TRUE(listener().did_change_battery_status());
  EXPECT_EQ(status.charging, received_status.charging);
  EXPECT_EQ(status.charging_time, received_status.chargingTime);
  EXPECT_EQ(status.discharging_time, received_status.dischargingTime);
  EXPECT_EQ(status.level, received_status.level);
}

}  // namespace content
