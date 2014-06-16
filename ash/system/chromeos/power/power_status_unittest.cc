// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/power/power_status.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {
namespace {

class TestObserver : public PowerStatus::Observer {
 public:
  TestObserver() : power_changed_count_(0) {}
  virtual ~TestObserver() {}

  int power_changed_count() const { return power_changed_count_; }

  // PowerStatus::Observer overrides:
  virtual void OnPowerStatusChanged() OVERRIDE { ++power_changed_count_; }

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

TEST_F(PowerStatusTest, InitializeAndUpdate) {
  // Test that the initial power supply state should be acquired after
  // PowerStatus is instantiated. This depends on
  // PowerManagerClientStubImpl, which responds to power status update
  // requests, pretends there is a battery present, and generates some valid
  // power supply status data.
  message_loop_.RunUntilIdle();
  EXPECT_EQ(1, test_observer_->power_changed_count());

  // Test RequestUpdate, test_obsever_ should be notified for power suuply
  // status change.
  power_status_->RequestStatusUpdate();
  message_loop_.RunUntilIdle();
  EXPECT_EQ(2, test_observer_->power_changed_count());
}

TEST_F(PowerStatusTest, ShouldDisplayBatteryTime) {
  EXPECT_FALSE(PowerStatus::ShouldDisplayBatteryTime(
      base::TimeDelta::FromSeconds(-1)));
  EXPECT_FALSE(PowerStatus::ShouldDisplayBatteryTime(
      base::TimeDelta::FromSeconds(0)));
  EXPECT_FALSE(PowerStatus::ShouldDisplayBatteryTime(
      base::TimeDelta::FromSeconds(59)));
  EXPECT_TRUE(PowerStatus::ShouldDisplayBatteryTime(
      base::TimeDelta::FromSeconds(60)));
  EXPECT_TRUE(PowerStatus::ShouldDisplayBatteryTime(
      base::TimeDelta::FromSeconds(600)));
  EXPECT_TRUE(PowerStatus::ShouldDisplayBatteryTime(
      base::TimeDelta::FromSeconds(3600)));
  EXPECT_TRUE(PowerStatus::ShouldDisplayBatteryTime(
      base::TimeDelta::FromSeconds(
          PowerStatus::kMaxBatteryTimeToDisplaySec)));
  EXPECT_FALSE(PowerStatus::ShouldDisplayBatteryTime(
      base::TimeDelta::FromSeconds(
          PowerStatus::kMaxBatteryTimeToDisplaySec + 1)));
}

TEST_F(PowerStatusTest, SplitTimeIntoHoursAndMinutes) {
  int hours = 0, minutes = 0;
  PowerStatus::SplitTimeIntoHoursAndMinutes(
      base::TimeDelta::FromSeconds(0), &hours, &minutes);
  EXPECT_EQ(0, hours);
  EXPECT_EQ(0, minutes);

  PowerStatus::SplitTimeIntoHoursAndMinutes(
      base::TimeDelta::FromSeconds(60), &hours, &minutes);
  EXPECT_EQ(0, hours);
  EXPECT_EQ(1, minutes);

  PowerStatus::SplitTimeIntoHoursAndMinutes(
      base::TimeDelta::FromSeconds(3600), &hours, &minutes);
  EXPECT_EQ(1, hours);
  EXPECT_EQ(0, minutes);

  PowerStatus::SplitTimeIntoHoursAndMinutes(
      base::TimeDelta::FromSeconds(3600 + 60), &hours, &minutes);
  EXPECT_EQ(1, hours);
  EXPECT_EQ(1, minutes);

  PowerStatus::SplitTimeIntoHoursAndMinutes(
      base::TimeDelta::FromSeconds(7 * 3600 + 23 * 60), &hours, &minutes);
  EXPECT_EQ(7, hours);
  EXPECT_EQ(23, minutes);

  // Check that minutes are rounded.
  PowerStatus::SplitTimeIntoHoursAndMinutes(
      base::TimeDelta::FromSeconds(2 * 3600 + 3 * 60 + 30), &hours, &minutes);
  EXPECT_EQ(2, hours);
  EXPECT_EQ(4, minutes);

  PowerStatus::SplitTimeIntoHoursAndMinutes(
      base::TimeDelta::FromSeconds(2 * 3600 + 3 * 60 + 29), &hours, &minutes);
  EXPECT_EQ(2, hours);
  EXPECT_EQ(3, minutes);

  // Check that times close to hour boundaries aren't incorrectly rounded such
  // that they display 60 minutes: http://crbug.com/368261
  PowerStatus::SplitTimeIntoHoursAndMinutes(
      base::TimeDelta::FromSecondsD(3599.9), &hours, &minutes);
  EXPECT_EQ(1, hours);
  EXPECT_EQ(0, minutes);

  PowerStatus::SplitTimeIntoHoursAndMinutes(
      base::TimeDelta::FromSecondsD(3600.1), &hours, &minutes);
  EXPECT_EQ(1, hours);
  EXPECT_EQ(0, minutes);
}

}  // namespace ash
