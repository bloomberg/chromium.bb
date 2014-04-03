// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/power/tray_power.h"

#include "ash/ash_switches.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "ui/message_center/fake_message_center.h"

using message_center::Notification;
using power_manager::PowerSupplyProperties;

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

class TrayPowerTest : public test::AshTestBase {
 public:
  TrayPowerTest() {}
  virtual ~TrayPowerTest() {}

  MockMessageCenter* message_center() { return message_center_.get(); }
  TrayPower* tray_power() { return tray_power_.get(); }

  // test::AshTestBase::SetUp() overrides:
  virtual void SetUp() OVERRIDE {
    test::AshTestBase::SetUp();
    message_center_.reset(new MockMessageCenter());
    tray_power_.reset(new TrayPower(NULL, message_center_.get()));
  }

  virtual void TearDown() OVERRIDE {
    tray_power_.reset();
    message_center_.reset();
    test::AshTestBase::TearDown();
  }

  TrayPower::NotificationState notification_state() const {
    return tray_power_->notification_state_;
  }

  bool MaybeShowUsbChargerNotification(const PowerSupplyProperties& proto) {
    PowerStatus::Get()->SetProtoForTesting(proto);
    return tray_power_->MaybeShowUsbChargerNotification();
  }

  bool UpdateNotificationState(const PowerSupplyProperties& proto) {
    PowerStatus::Get()->SetProtoForTesting(proto);
    return tray_power_->UpdateNotificationState();
  }

  void SetUsbChargerConnected(bool connected) {
    tray_power_->usb_charger_was_connected_ = connected;
   }

  // Returns a discharging PowerSupplyProperties more appropriate for testing.
  static PowerSupplyProperties DefaultPowerSupplyProperties() {
    PowerSupplyProperties proto;
    proto.set_external_power(
        power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);
    proto.set_battery_state(
        power_manager::PowerSupplyProperties_BatteryState_DISCHARGING);
    proto.set_battery_percent(50.0);
    proto.set_battery_time_to_empty_sec(3 * 60 * 60);
    proto.set_battery_time_to_full_sec(2 * 60 * 60);
    proto.set_is_calculating_battery_time(false);
    return proto;
  }

 private:
  scoped_ptr<MockMessageCenter> message_center_;
  scoped_ptr<TrayPower> tray_power_;

  DISALLOW_COPY_AND_ASSIGN(TrayPowerTest);
};

TEST_F(TrayPowerTest, MaybeShowUsbChargerNotification) {
  PowerSupplyProperties discharging = DefaultPowerSupplyProperties();
  EXPECT_FALSE(MaybeShowUsbChargerNotification(discharging));
  EXPECT_EQ(0, message_center()->add_count());
  EXPECT_EQ(0, message_center()->remove_count());

  // Notification shows when connecting a USB charger.
  PowerSupplyProperties usb_connected = DefaultPowerSupplyProperties();
  usb_connected.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_USB);
  EXPECT_TRUE(MaybeShowUsbChargerNotification(usb_connected));
  EXPECT_EQ(1, message_center()->add_count());
  EXPECT_EQ(0, message_center()->remove_count());

  // Change in charge does not trigger the notification again.
  PowerSupplyProperties more_charge = DefaultPowerSupplyProperties();
  more_charge.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_USB);
  more_charge.set_battery_time_to_full_sec(60 * 60);
  more_charge.set_battery_percent(75.0);
  SetUsbChargerConnected(true);
  EXPECT_FALSE(MaybeShowUsbChargerNotification(more_charge));
  EXPECT_EQ(1, message_center()->add_count());
  EXPECT_EQ(0, message_center()->remove_count());

  // Disconnecting a USB charger with the notification showing should close
  // the notification.
  EXPECT_TRUE(MaybeShowUsbChargerNotification(discharging));
  EXPECT_EQ(1, message_center()->add_count());
  EXPECT_EQ(1, message_center()->remove_count());
}

TEST_F(TrayPowerTest, UpdateNotificationState) {
  // No notifications when no battery present.
  PowerSupplyProperties no_battery = DefaultPowerSupplyProperties();
  no_battery.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_AC);
  no_battery.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_NOT_PRESENT);
  EXPECT_FALSE(UpdateNotificationState(no_battery));
  EXPECT_EQ(TrayPower::NOTIFICATION_NONE, notification_state());

  // No notification when calculating remaining battery time.
  PowerSupplyProperties calculating = DefaultPowerSupplyProperties();
  calculating.set_is_calculating_battery_time(true);
  EXPECT_FALSE(UpdateNotificationState(calculating));
  EXPECT_EQ(TrayPower::NOTIFICATION_NONE, notification_state());

  // No notification when charging.
  PowerSupplyProperties charging = DefaultPowerSupplyProperties();
  charging.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_AC);
  charging.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_CHARGING);
  EXPECT_FALSE(UpdateNotificationState(charging));
  EXPECT_EQ(TrayPower::NOTIFICATION_NONE, notification_state());

  // When the rounded minutes-to-empty are above the threshold, no notification
  // should be shown.
  PowerSupplyProperties low = DefaultPowerSupplyProperties();
  low.set_battery_time_to_empty_sec(TrayPower::kLowPowerMinutes * 60 + 30);
  EXPECT_FALSE(UpdateNotificationState(low));
  EXPECT_EQ(TrayPower::NOTIFICATION_NONE, notification_state());

  // When the rounded value matches the threshold, the notification should
  // appear.
  low.set_battery_time_to_empty_sec(TrayPower::kLowPowerMinutes * 60 + 29);
  EXPECT_TRUE(UpdateNotificationState(low));
  EXPECT_EQ(TrayPower::NOTIFICATION_LOW_POWER, notification_state());

  // It should persist at lower values.
  low.set_battery_time_to_empty_sec(TrayPower::kLowPowerMinutes * 60 - 20);
  EXPECT_FALSE(UpdateNotificationState(low));
  EXPECT_EQ(TrayPower::NOTIFICATION_LOW_POWER, notification_state());

  // The critical low battery notification should be shown when the rounded
  // value is at the lower threshold.
  PowerSupplyProperties critical = DefaultPowerSupplyProperties();
  critical.set_battery_time_to_empty_sec(TrayPower::kCriticalMinutes * 60 + 29);
  EXPECT_TRUE(UpdateNotificationState(critical));
  EXPECT_EQ(TrayPower::NOTIFICATION_CRITICAL, notification_state());

  // The notification should be dismissed when the no-warning threshold is
  // reached.
  PowerSupplyProperties safe = DefaultPowerSupplyProperties();
  safe.set_battery_time_to_empty_sec(TrayPower::kNoWarningMinutes * 60 - 29);
  EXPECT_FALSE(UpdateNotificationState(safe));
  EXPECT_EQ(TrayPower::NOTIFICATION_NONE, notification_state());

  // Test that rounded percentages are used when a USB charger is connected.
  PowerSupplyProperties low_usb = DefaultPowerSupplyProperties();
  low_usb.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_USB);
  low_usb.set_battery_percent(TrayPower::kLowPowerPercentage + 0.5);
  EXPECT_FALSE(UpdateNotificationState(low_usb));
  EXPECT_EQ(TrayPower::NOTIFICATION_NONE, notification_state());

  low_usb.set_battery_percent(TrayPower::kLowPowerPercentage + 0.49);
  EXPECT_TRUE(UpdateNotificationState(low_usb));
  EXPECT_EQ(TrayPower::NOTIFICATION_LOW_POWER, notification_state());

  PowerSupplyProperties critical_usb = DefaultPowerSupplyProperties();
  critical_usb.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_USB);
  critical_usb.set_battery_percent(TrayPower::kCriticalPercentage + 0.2);
  EXPECT_TRUE(UpdateNotificationState(critical_usb));
  EXPECT_EQ(TrayPower::NOTIFICATION_CRITICAL, notification_state());

  PowerSupplyProperties safe_usb = DefaultPowerSupplyProperties();
  safe_usb.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_USB);
  safe_usb.set_battery_percent(TrayPower::kNoWarningPercentage - 0.1);
  EXPECT_FALSE(UpdateNotificationState(safe_usb));
  EXPECT_EQ(TrayPower::NOTIFICATION_NONE, notification_state());

  // A notification shouldn't be shown when we're in the full state with an
  // original Spring charger connected: http://crbug.com/338376
  PowerSupplyProperties spring = DefaultPowerSupplyProperties();
  spring.set_external_power(power_manager::
      PowerSupplyProperties_ExternalPower_ORIGINAL_SPRING_CHARGER);
  spring.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_FULL);
  spring.set_battery_time_to_empty_sec(0);
  spring.set_battery_time_to_full_sec(0);
  EXPECT_FALSE(UpdateNotificationState(spring));
  EXPECT_EQ(TrayPower::NOTIFICATION_NONE, notification_state());
}

}  // namespace ash
