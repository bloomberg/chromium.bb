// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/power/power_status_view.h"

#include "ash/system/chromeos/power/power_status.h"
#include "ash/test/ash_test_base.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/views/controls/label.h"

using power_manager::PowerSupplyProperties;

namespace ash {

class PowerStatusViewTest : public test::AshTestBase {
 public:
  PowerStatusViewTest() {}
  virtual ~PowerStatusViewTest() {}

  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    test::AshTestBase::SetUp();
    view_.reset(new PowerStatusView(GetViewType(), false));
  }

  virtual void TearDown() OVERRIDE {
    view_.reset();
    test::AshTestBase::TearDown();
  }

 protected:
  virtual PowerStatusView::ViewType GetViewType() = 0;
  PowerStatusView* view() { return view_.get(); }

  void UpdatePowerStatus(const power_manager::PowerSupplyProperties& proto) {
    PowerStatus::Get()->SetProtoForTesting(proto);
    view_->OnPowerStatusChanged();
  }

 private:
  scoped_ptr<PowerStatusView> view_;

  DISALLOW_COPY_AND_ASSIGN(PowerStatusViewTest);
};

class PowerStatusDefaultViewTest : public PowerStatusViewTest {
 public:
  PowerStatusDefaultViewTest() {}
  virtual ~PowerStatusDefaultViewTest() {}

 protected:
  virtual PowerStatusView::ViewType GetViewType() OVERRIDE {
    return PowerStatusView::VIEW_DEFAULT;
  }

  bool IsPercentageVisible() {
    return view()->percentage_label_->visible();
  }

  bool IsTimeStatusVisible() {
    return view()->time_status_label_->visible();
  }

  base::string16 RemainingTimeInView() {
    return view()->time_status_label_->text();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PowerStatusDefaultViewTest);
};

class PowerStatusNotificationViewTest : public PowerStatusViewTest {
 public:
  PowerStatusNotificationViewTest() {}
  virtual ~PowerStatusNotificationViewTest() {}

 protected:
  virtual PowerStatusView::ViewType GetViewType() OVERRIDE {
    return PowerStatusView::VIEW_NOTIFICATION;
  }

  base::string16 StatusInView() {
    return view()->status_label_->text();
  }

  base::string16 RemainingTimeInView() {
    return view()->time_label_->text();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PowerStatusNotificationViewTest);
};

TEST_F(PowerStatusDefaultViewTest, Basic) {
  EXPECT_FALSE(IsPercentageVisible());
  EXPECT_TRUE(IsTimeStatusVisible());

  // Disconnect the power.
  PowerSupplyProperties prop;
  prop.set_external_power(PowerSupplyProperties::DISCONNECTED);
  prop.set_battery_state(PowerSupplyProperties::DISCHARGING);
  prop.set_battery_percent(99.0);
  prop.set_battery_time_to_empty_sec(120);
  prop.set_is_calculating_battery_time(true);
  UpdatePowerStatus(prop);

  EXPECT_TRUE(IsPercentageVisible());
  EXPECT_TRUE(IsTimeStatusVisible());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BATTERY_CALCULATING),
            RemainingTimeInView());

  prop.set_is_calculating_battery_time(false);
  UpdatePowerStatus(prop);
  EXPECT_NE(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BATTERY_CALCULATING),
            RemainingTimeInView());
  EXPECT_NE(
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_BATTERY_CHARGING_UNRELIABLE),
      RemainingTimeInView());

  prop.set_external_power(PowerSupplyProperties::AC);
  prop.set_battery_state(PowerSupplyProperties::CHARGING);
  prop.set_battery_time_to_full_sec(120);
  UpdatePowerStatus(prop);
  EXPECT_TRUE(IsPercentageVisible());
  EXPECT_TRUE(IsTimeStatusVisible());
  EXPECT_NE(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BATTERY_CALCULATING),
            RemainingTimeInView());
  EXPECT_NE(
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_BATTERY_CHARGING_UNRELIABLE),
      RemainingTimeInView());

  prop.set_external_power(PowerSupplyProperties::USB);
  UpdatePowerStatus(prop);
  EXPECT_TRUE(IsPercentageVisible());
  EXPECT_TRUE(IsTimeStatusVisible());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_BATTERY_CHARGING_UNRELIABLE),
      RemainingTimeInView());

  // Tricky -- connected to non-USB but still discharging. Not likely happening
  // on production though.
  prop.set_external_power(PowerSupplyProperties::AC);
  prop.set_battery_state(PowerSupplyProperties::DISCHARGING);
  prop.set_battery_time_to_full_sec(120);
  UpdatePowerStatus(prop);
  EXPECT_TRUE(IsPercentageVisible());
  EXPECT_FALSE(IsTimeStatusVisible());
}

TEST_F(PowerStatusNotificationViewTest, Basic) {
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BATTERY_FULL),
            StatusInView());
  EXPECT_TRUE(RemainingTimeInView().empty());

  // Disconnect the power.
  PowerSupplyProperties prop;
  prop.set_external_power(PowerSupplyProperties::DISCONNECTED);
  prop.set_battery_state(PowerSupplyProperties::DISCHARGING);
  prop.set_battery_percent(99.0);
  prop.set_battery_time_to_empty_sec(125);
  prop.set_is_calculating_battery_time(true);
  UpdatePowerStatus(prop);

  EXPECT_NE(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BATTERY_FULL),
            StatusInView());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BATTERY_CALCULATING),
            RemainingTimeInView());

  prop.set_is_calculating_battery_time(false);
  UpdatePowerStatus(prop);
  // Low power warning has to be calculated by ui::TimeFormat, but ignore
  // seconds.
  EXPECT_EQ(ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_REMAINING,
                                   ui::TimeFormat::LENGTH_LONG,
                                   base::TimeDelta::FromMinutes(2)),
            RemainingTimeInView());

  prop.set_external_power(PowerSupplyProperties::AC);
  prop.set_battery_state(PowerSupplyProperties::CHARGING);
  prop.set_battery_time_to_full_sec(120);
  UpdatePowerStatus(prop);
  EXPECT_NE(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BATTERY_FULL),
            StatusInView());
  // Charging time is somehow using another format?
  EXPECT_NE(ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_REMAINING,
                                   ui::TimeFormat::LENGTH_LONG,
                                   base::TimeDelta::FromMinutes(2)),
            RemainingTimeInView());

  // Unreliable connection.
  prop.set_external_power(PowerSupplyProperties::USB);
  UpdatePowerStatus(prop);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_BATTERY_CHARGING_UNRELIABLE),
      RemainingTimeInView());

  // Tricky -- connected to non-USB but still discharging. Not likely happening
  // on production though.
  prop.set_external_power(PowerSupplyProperties::AC);
  prop.set_battery_state(PowerSupplyProperties::DISCHARGING);
  prop.set_battery_time_to_full_sec(120);
  UpdatePowerStatus(prop);
  EXPECT_TRUE(RemainingTimeInView().empty());
}

}  // namespace ash
