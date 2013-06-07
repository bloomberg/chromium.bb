// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/power/power_manager_handler.h"

#include <set>
#include <string>

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

void ValidatePowerSupplyStatus(const PowerSupplyStatus& status) {
  EXPECT_TRUE(status.battery_is_present);
  EXPECT_GT(status.battery_percentage, 0.0);
  if (status.battery_state == PowerSupplyStatus::DISCHARGING) {
    EXPECT_GT(status.battery_seconds_to_empty, 0);
    EXPECT_EQ(status.battery_seconds_to_full, 0);
  } else {
    EXPECT_GT(status.battery_seconds_to_full, 0);
    EXPECT_EQ(status.battery_seconds_to_empty, 0);
  }
}

class TestObserver : public PowerManagerHandler::Observer {
 public:
  TestObserver() : power_changed_count_(0) {
  }
  virtual ~TestObserver() {}

  virtual void OnPowerStatusChanged(
      const PowerSupplyStatus& power_status) OVERRIDE {
    ++power_changed_count_;
    ValidatePowerSupplyStatus(power_status);
  }

  int power_changed_count() const { return power_changed_count_; }

 private:
  int power_changed_count_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

}  // namespace

class PowerManagerHandlerTest : public testing::Test {
 public:
  PowerManagerHandlerTest() {}
  virtual ~PowerManagerHandlerTest() {}

  virtual void SetUp() OVERRIDE {
    // Initialize DBusThreadManager with a stub implementation.
    DBusThreadManager::InitializeWithStub();

    power_handler_.reset(new PowerManagerHandler);
    test_observer_.reset(new TestObserver);
    power_handler_->AddObserver(test_observer_.get());
  }

  virtual void TearDown() OVERRIDE {
    power_handler_.reset();
    test_observer_.reset();
    DBusThreadManager::Shutdown();
  }

 protected:
  base::MessageLoopForUI message_loop_;
  scoped_ptr<PowerManagerHandler> power_handler_;
  scoped_ptr<TestObserver> test_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PowerManagerHandlerTest);
};

TEST_F(PowerManagerHandlerTest, PowerManagerInitializeAndUpdate) {
  // Test that the initial power supply state should be acquired after
  // PowerManagerHandler is instantiated. This depends on
  // PowerManagerClientStubImpl, which responds to power status update
  // requests, pretends there is a battery present, and generate some
  // valid power supply status data.
  message_loop_.RunUntilIdle();
  const int initial_changed_count = test_observer_->power_changed_count();
  PowerSupplyStatus init_status = power_handler_->GetPowerSupplyStatus();
  ValidatePowerSupplyStatus(init_status);

  // Test RequestUpdate, test_obsever_ should be notified for power suuply
  // status change.
  power_handler_->RequestStatusUpdate();
  message_loop_.RunUntilIdle();
  EXPECT_GT(test_observer_->power_changed_count(), initial_changed_count);
}

}  // namespace chromeos
