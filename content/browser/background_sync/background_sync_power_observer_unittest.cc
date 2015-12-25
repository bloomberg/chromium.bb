// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <content/browser/background_sync/background_sync_power_observer.h>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/power_monitor_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class BackgroundSyncPowerObserverTest : public testing::Test {
 protected:
  BackgroundSyncPowerObserverTest() : power_changed_count_(0) {
    power_monitor_source_ = new base::PowerMonitorTestSource();
    power_monitor_.reset(new base::PowerMonitor(
        scoped_ptr<base::PowerMonitorSource>(power_monitor_source_)));
    power_observer_.reset(new BackgroundSyncPowerObserver(
        base::Bind(&BackgroundSyncPowerObserverTest::OnPowerChanged,
                   base::Unretained(this))));
  }

  void SetOnBatteryPower(bool on_battery_power) {
    power_monitor_source_->GeneratePowerStateEvent(on_battery_power);
  }

  void OnPowerChanged() { power_changed_count_++; }

  // power_monitor_source_ is owned by power_monitor_
  base::PowerMonitorTestSource* power_monitor_source_;
  scoped_ptr<base::PowerMonitor> power_monitor_;
  scoped_ptr<BackgroundSyncPowerObserver> power_observer_;
  int power_changed_count_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncPowerObserverTest);
};

TEST_F(BackgroundSyncPowerObserverTest, PowerChangeInvokesCallback) {
  SetOnBatteryPower(true);
  power_changed_count_ = 0;

  SetOnBatteryPower(false);
  EXPECT_EQ(1, power_changed_count_);
  SetOnBatteryPower(true);
  EXPECT_EQ(2, power_changed_count_);
  SetOnBatteryPower(true);
  EXPECT_EQ(2, power_changed_count_);
}

TEST_F(BackgroundSyncPowerObserverTest, PowerSufficientAuto) {
  SetOnBatteryPower(false);
  EXPECT_TRUE(power_observer_->PowerSufficient(POWER_STATE_AUTO));

  SetOnBatteryPower(true);
  EXPECT_TRUE(power_observer_->PowerSufficient(POWER_STATE_AUTO));
}

TEST_F(BackgroundSyncPowerObserverTest, PowerSufficientAvoidDraining) {
  SetOnBatteryPower(false);
  EXPECT_TRUE(power_observer_->PowerSufficient(POWER_STATE_AVOID_DRAINING));

  SetOnBatteryPower(true);
  EXPECT_FALSE(power_observer_->PowerSufficient(POWER_STATE_AVOID_DRAINING));
}

}  // namespace

}  // namespace content
