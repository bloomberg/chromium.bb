// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/power/tray_power.h"

#include "ash/ash_switches.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/fake_message_center.h"

using chromeos::PowerSupplyStatus;
using message_center::Notification;

namespace {

class MockMessageCenter : public message_center::FakeMessageCenter {
 public:
  MockMessageCenter() : add_count_(0), remove_count_(0) {}
  virtual ~MockMessageCenter() {}

  int add_count() const { return add_count_; }
  int remove_count() const { return remove_count_; }

  // message_center::FakeMessageCenter overrides:
  virtual void AddNotification(scoped_ptr<Notification> notification) OVERRIDE {
    add_count_++;
  }
  virtual void RemoveNotification(const std::string& id, bool by_user)
      OVERRIDE {
    remove_count_++;
  }

 private:
  int add_count_;
  int remove_count_;

  DISALLOW_COPY_AND_ASSIGN(MockMessageCenter);
};

}  // namespace

namespace ash {
namespace internal {

class TrayPowerTest : public testing::Test {
 public:
  TrayPowerTest() {}
  virtual ~TrayPowerTest() {}

  MockMessageCenter* message_center() { return message_center_.get(); }
  TrayPower* tray_power() { return tray_power_.get(); }

  // testing::Test overrides:
  virtual void SetUp() OVERRIDE {
    message_center_.reset(new MockMessageCenter());
    tray_power_.reset(new TrayPower(NULL, message_center_.get()));
  }

  TrayPower::NotificationState notification_state() const {
    return tray_power_->notification_state_;
  }

  bool MaybeShowUsbChargerNotification(const PowerSupplyStatus& old_status,
                                       const PowerSupplyStatus& new_status) {
    return tray_power_->MaybeShowUsbChargerNotification(old_status, new_status);
  }

  bool UpdateNotificationState(const PowerSupplyStatus& status) {
    return tray_power_->UpdateNotificationState(status);
  }

  void SetLastPowerStatus(const PowerSupplyStatus& status) {
    tray_power_->last_power_supply_status_ = status;
  }

  // Returns a discharging PowerSupplyStatus more appropriate for testing.
  static PowerSupplyStatus DefaultPowerSupplyStatus() {
    PowerSupplyStatus status;
    status.line_power_on = false;
    status.battery_is_present = true;
    status.battery_is_full = false;
    status.battery_seconds_to_empty = 3 * 60 * 60;
    status.battery_seconds_to_full = 2 * 60 * 60;
    status.battery_percentage = 50.0;
    status.is_calculating_battery_time = false;
    status.battery_state = PowerSupplyStatus::DISCHARGING;
    return status;
  }

 private:
  scoped_ptr<MockMessageCenter> message_center_;
  scoped_ptr<TrayPower> tray_power_;

  DISALLOW_COPY_AND_ASSIGN(TrayPowerTest);
};

TEST_F(TrayPowerTest, MaybeShowUsbChargerNotification) {
  // Notification shows when connecting a USB charger.
  PowerSupplyStatus discharging = DefaultPowerSupplyStatus();
  PowerSupplyStatus usb_connected = DefaultPowerSupplyStatus();
  usb_connected.line_power_on = true;
  usb_connected.battery_state = PowerSupplyStatus::CONNECTED_TO_USB;
  EXPECT_TRUE(MaybeShowUsbChargerNotification(discharging, usb_connected));
  EXPECT_EQ(1, message_center()->add_count());

  // Change in charge does not trigger the notification again.
  PowerSupplyStatus more_charge = DefaultPowerSupplyStatus();
  more_charge.line_power_on = true;
  more_charge.battery_seconds_to_full = 60 * 60;
  more_charge.battery_percentage = 75.0;
  more_charge.battery_state = PowerSupplyStatus::CONNECTED_TO_USB;
  EXPECT_FALSE(MaybeShowUsbChargerNotification(usb_connected, more_charge));
  EXPECT_EQ(1, message_center()->add_count());
  EXPECT_EQ(0, message_center()->remove_count());

  // Disconnecting a USB charger with the notification showing should close
  // the notification.
  EXPECT_TRUE(MaybeShowUsbChargerNotification(usb_connected, discharging));
  EXPECT_EQ(1, message_center()->remove_count());
}

TEST_F(TrayPowerTest, UpdateNotificationState) {
  // No notifications when no battery present.
  PowerSupplyStatus no_battery = DefaultPowerSupplyStatus();
  no_battery.battery_is_present = false;
  EXPECT_FALSE(UpdateNotificationState(no_battery));
  EXPECT_EQ(TrayPower::NOTIFICATION_NONE, notification_state());

  // No notification when calculating remaining battery time.
  PowerSupplyStatus calculating = DefaultPowerSupplyStatus();
  calculating.is_calculating_battery_time = true;
  EXPECT_FALSE(UpdateNotificationState(calculating));
  EXPECT_EQ(TrayPower::NOTIFICATION_NONE, notification_state());

  // No notification when charging.
  PowerSupplyStatus charging = DefaultPowerSupplyStatus();
  charging.line_power_on = true;
  charging.battery_state = PowerSupplyStatus::CHARGING;
  EXPECT_FALSE(UpdateNotificationState(charging));
  EXPECT_EQ(TrayPower::NOTIFICATION_NONE, notification_state());

  // Critical low battery notification.
  PowerSupplyStatus critical = DefaultPowerSupplyStatus();
  critical.battery_seconds_to_empty = 60;
  critical.battery_percentage = 2.0;
  EXPECT_TRUE(UpdateNotificationState(critical));
  EXPECT_EQ(TrayPower::NOTIFICATION_CRITICAL, notification_state());
}

}  // namespace internal
}  // namespace ash
