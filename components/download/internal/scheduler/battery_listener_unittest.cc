// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/scheduler/battery_listener.h"

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/test/power_monitor_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace download {

using BatteryRequirements = SchedulingParams::BatteryRequirements;

class MockObserver : public BatteryListener::Observer {
 public:
  MOCK_METHOD1(OnBatteryChange, void(SchedulingParams::BatteryRequirements));
};

class BatteryListenerTest : public testing::Test {
 public:
  BatteryListenerTest() {}
  ~BatteryListenerTest() override = default;

  void CallBatteryChange(bool on_battery_power) {
    DCHECK(listener_);
    static_cast<base::PowerObserver*>(listener_.get())
        ->OnPowerStateChange(on_battery_power);
  }

  void SetUp() override {
    power_monitor_ = base::MakeUnique<base::PowerMonitor>(
        base::MakeUnique<base::PowerMonitorTestSource>());

    observer_ = base::MakeUnique<MockObserver>();
    listener_ = base::MakeUnique<BatteryListener>();
    listener_->AddObserver(observer_.get());
  }

  void TearDown() override {
    listener_.reset();
    observer_.reset();
  }

  std::unique_ptr<BatteryListener> listener_;
  std::unique_ptr<MockObserver> observer_;

  base::MessageLoop message_loop_;
  std::unique_ptr<base::PowerMonitor> power_monitor_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BatteryListenerTest);
};

// Ensures observer methods are corrected called.
TEST_F(BatteryListenerTest, NotifyObservers) {
  listener_->Start();
  EXPECT_CALL(*observer_.get(),
              OnBatteryChange(BatteryRequirements::BATTERY_INSENSITIVE))
      .RetiresOnSaturation();
  CallBatteryChange(true);

  EXPECT_CALL(*observer_.get(),
              OnBatteryChange(BatteryRequirements::BATTERY_SENSITIVE))
      .RetiresOnSaturation();
  CallBatteryChange(false);
  listener_->Stop();
};

}  // namespace download
