// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/power/power_status.h"

#include <set>
#include <string>

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_supply_status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {
namespace internal {

namespace {

void ValidatePowerSupplyStatus(const chromeos::PowerSupplyStatus& status) {
  EXPECT_TRUE(status.battery_is_present);
  EXPECT_GT(status.battery_percentage, 0.0);
  if (status.battery_state == chromeos::PowerSupplyStatus::DISCHARGING) {
    EXPECT_GT(status.battery_seconds_to_empty, 0);
    EXPECT_EQ(status.battery_seconds_to_full, 0);
  } else {
    EXPECT_GT(status.battery_seconds_to_full, 0);
    EXPECT_EQ(status.battery_seconds_to_empty, 0);
  }
}

class TestObserver : public PowerStatus::Observer {
 public:
  TestObserver() : power_changed_count_(0) {
  }
  virtual ~TestObserver() {}

  virtual void OnPowerStatusChanged(
      const chromeos::PowerSupplyStatus& power_status) OVERRIDE {
    ++power_changed_count_;
    ValidatePowerSupplyStatus(power_status);
  }

  int power_changed_count() const { return power_changed_count_; }

 private:
  int power_changed_count_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

}  // namespace

class PowerStatusTest : public testing::Test {
 public:
  PowerStatusTest() : power_status_(NULL) {}
  virtual ~PowerStatusTest() {}

  virtual void SetUp() OVERRIDE {
    chromeos::DBusThreadManager::InitializeWithStub();
    PowerStatus::Initialize();
    power_status_ = PowerStatus::Get();
    test_observer_.reset(new TestObserver);
    power_status_->AddObserver(test_observer_.get());
  }

  virtual void TearDown() OVERRIDE {
    power_status_->RemoveObserver(test_observer_.get());
    test_observer_.reset();
    PowerStatus::Shutdown();
    chromeos::DBusThreadManager::Shutdown();
  }

 protected:
  base::MessageLoopForUI message_loop_;
  PowerStatus* power_status_;  // Not owned.
  scoped_ptr<TestObserver> test_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PowerStatusTest);
};

TEST_F(PowerStatusTest, PowerStatusInitializeAndUpdate) {
  // Test that the initial power supply state should be acquired after
  // PowerStatus is instantiated. This depends on
  // PowerManagerClientStubImpl, which responds to power status update
  // requests, pretends there is a battery present, and generates some valid
  // power supply status data.
  message_loop_.RunUntilIdle();
  EXPECT_EQ(1, test_observer_->power_changed_count());
  chromeos::PowerSupplyStatus init_status =
      power_status_->GetPowerSupplyStatus();
  ValidatePowerSupplyStatus(init_status);

  // Test RequestUpdate, test_obsever_ should be notified for power suuply
  // status change.
  power_status_->RequestStatusUpdate();
  message_loop_.RunUntilIdle();
  EXPECT_EQ(2, test_observer_->power_changed_count());
}

}  // namespace internal
}  // namespace ash
