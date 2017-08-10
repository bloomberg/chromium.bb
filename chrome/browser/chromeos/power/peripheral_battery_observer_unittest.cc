// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/peripheral_battery_observer.h"

#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/events/test/device_data_manager_test_api.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"

namespace {

const char kTestBatteryPath[] = "/sys/class/power_supply/hid-AA:BB:CC-battery";
const char kTestBatteryAddress[] = "cc:bb:aa";
const char kTestDeviceName[] = "test device";

}  // namespace

namespace chromeos {

class PeripheralBatteryObserverTest : public ash::AshTestBase {
 public:
  PeripheralBatteryObserverTest() {}
  ~PeripheralBatteryObserverTest() override {}

  void SetUp() override {
    ash::AshTestBase::SetUp();
    observer_ = base::MakeUnique<PeripheralBatteryObserver>();
  }

  void TearDown() override {
    observer_.reset();
    ash::AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<PeripheralBatteryObserver> observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PeripheralBatteryObserverTest);
};

TEST_F(PeripheralBatteryObserverTest, Basic) {
  base::SimpleTestTickClock clock;
  observer_->set_testing_clock(&clock);

  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();

  // Level 50 at time 100, no low-battery notification.
  clock.Advance(base::TimeDelta::FromSeconds(100));
  observer_->PeripheralBatteryStatusReceived(kTestBatteryPath, kTestDeviceName,
                                             50);
  EXPECT_EQ(1u, observer_->batteries_.count(kTestBatteryAddress));

  const PeripheralBatteryObserver::BatteryInfo& info =
      observer_->batteries_[kTestBatteryAddress];

  EXPECT_EQ(kTestDeviceName, info.name);
  EXPECT_EQ(50, info.level);
  EXPECT_EQ(base::TimeTicks(), info.last_notification_timestamp);
  EXPECT_TRUE(message_center->FindVisibleNotificationById(
                  kTestBatteryAddress) == nullptr);

  // Level 5 at time 110, low-battery notification.
  clock.Advance(base::TimeDelta::FromSeconds(10));
  observer_->PeripheralBatteryStatusReceived(kTestBatteryPath, kTestDeviceName,
                                             5);
  EXPECT_EQ(5, info.level);
  EXPECT_EQ(clock.NowTicks(), info.last_notification_timestamp);
  EXPECT_TRUE(message_center->FindVisibleNotificationById(
                  kTestBatteryAddress) != nullptr);

  // Verify that the low-battery notification for stylus does not show up.
  EXPECT_FALSE(message_center->FindVisibleNotificationById(
                   PeripheralBatteryObserver::kStylusNotificationId) !=
               nullptr);

  // Level -1 at time 115, cancel previous notification
  clock.Advance(base::TimeDelta::FromSeconds(5));
  observer_->PeripheralBatteryStatusReceived(kTestBatteryPath, kTestDeviceName,
                                             -1);
  EXPECT_EQ(5, info.level);
  EXPECT_EQ(clock.NowTicks() - base::TimeDelta::FromSeconds(5),
            info.last_notification_timestamp);
  EXPECT_TRUE(message_center->FindVisibleNotificationById(
                  kTestBatteryAddress) == nullptr);

  // Level 50 at time 120, no low-battery notification.
  clock.Advance(base::TimeDelta::FromSeconds(5));
  observer_->PeripheralBatteryStatusReceived(kTestBatteryPath, kTestDeviceName,
                                             50);
  EXPECT_EQ(50, info.level);
  EXPECT_EQ(clock.NowTicks() - base::TimeDelta::FromSeconds(10),
            info.last_notification_timestamp);
  EXPECT_TRUE(message_center->FindVisibleNotificationById(
                  kTestBatteryAddress) == nullptr);

  // Level 5 at time 130, no low-battery notification (throttling).
  clock.Advance(base::TimeDelta::FromSeconds(10));
  observer_->PeripheralBatteryStatusReceived(kTestBatteryPath, kTestDeviceName,
                                             5);
  EXPECT_EQ(5, info.level);
  EXPECT_EQ(clock.NowTicks() - base::TimeDelta::FromSeconds(20),
            info.last_notification_timestamp);
  EXPECT_TRUE(message_center->FindVisibleNotificationById(
                  kTestBatteryAddress) == nullptr);
}

TEST_F(PeripheralBatteryObserverTest, InvalidBatteryInfo) {
  observer_->PeripheralBatteryStatusReceived("invalid-path", kTestDeviceName,
                                             10);
  EXPECT_TRUE(observer_->batteries_.empty());

  observer_->PeripheralBatteryStatusReceived(
      "/sys/class/power_supply/hid-battery", kTestDeviceName, 10);
  EXPECT_TRUE(observer_->batteries_.empty());

  observer_->PeripheralBatteryStatusReceived(kTestBatteryPath, kTestDeviceName,
                                             -2);
  EXPECT_TRUE(observer_->batteries_.empty());

  observer_->PeripheralBatteryStatusReceived(kTestBatteryPath, kTestDeviceName,
                                             101);
  EXPECT_TRUE(observer_->batteries_.empty());

  observer_->PeripheralBatteryStatusReceived(kTestBatteryPath, kTestDeviceName,
                                             -1);
  EXPECT_TRUE(observer_->batteries_.empty());
}

TEST_F(PeripheralBatteryObserverTest, DeviceRemove) {
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();

  observer_->PeripheralBatteryStatusReceived(kTestBatteryPath, kTestDeviceName,
                                             5);
  EXPECT_EQ(1u, observer_->batteries_.count(kTestBatteryAddress));
  EXPECT_TRUE(message_center->FindVisibleNotificationById(
                  kTestBatteryAddress) != nullptr);

  observer_->RemoveBattery(kTestBatteryAddress);
  EXPECT_TRUE(message_center->FindVisibleNotificationById(
                  kTestBatteryAddress) == nullptr);
}

TEST_F(PeripheralBatteryObserverTest, StylusNotification) {
  const std::string kTestStylusName = "test_stylus";

  // Add an external stylus to our test device manager.
  ui::TouchscreenDevice stylus(0 /* id */, ui::INPUT_DEVICE_EXTERNAL,
                               kTestStylusName, gfx::Size(),
                               1 /* touch_points */);
  stylus.sys_path = base::FilePath(kTestBatteryPath);
  stylus.is_stylus = true;

  ui::test::DeviceDataManagerTestAPI test_api;
  test_api.SetTouchscreenDevices({stylus});

  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();

  // Verify that when the battery level is 50, no stylus low battery
  // notification is shown.
  observer_->PeripheralBatteryStatusReceived(kTestBatteryPath, kTestStylusName,
                                             50);
  EXPECT_TRUE(message_center->FindVisibleNotificationById(
                  PeripheralBatteryObserver::kStylusNotificationId) == nullptr);

  // Verify that when the battery level is 5, a stylus low battery notification
  // is shown. Also check that a non stylus device low battery notification will
  // not show up.
  observer_->PeripheralBatteryStatusReceived(kTestBatteryPath, kTestStylusName,
                                             5);
  EXPECT_TRUE(message_center->FindVisibleNotificationById(
                  PeripheralBatteryObserver::kStylusNotificationId) != nullptr);
  EXPECT_TRUE(message_center->FindVisibleNotificationById(
                  kTestBatteryAddress) == nullptr);

  // Verify that when the battery level is -1, the previous stylus low battery
  // notification is cancelled.
  observer_->PeripheralBatteryStatusReceived(kTestBatteryPath, kTestStylusName,
                                             -1);
  EXPECT_TRUE(message_center->FindVisibleNotificationById(
                  PeripheralBatteryObserver::kStylusNotificationId) == nullptr);
}

}  // namespace chromeos
