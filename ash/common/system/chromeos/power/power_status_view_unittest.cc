// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/power/power_status_view.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/system/chromeos/power/power_status.h"
#include "ash/test/ash_md_test_base.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

using power_manager::PowerSupplyProperties;

namespace ash {

class PowerStatusViewTest : public test::AshMDTestBase {
 public:
  PowerStatusViewTest() {}
  ~PowerStatusViewTest() override {}

  // Overridden from testing::Test:
  void SetUp() override {
    test::AshMDTestBase::SetUp();
    view_.reset(new PowerStatusView(false));
  }

  void TearDown() override {
    view_.reset();
    test::AshMDTestBase::TearDown();
  }

 protected:
  void UpdatePowerStatus(const power_manager::PowerSupplyProperties& proto) {
    PowerStatus::Get()->SetProtoForTesting(proto);
    view_->OnPowerStatusChanged();
  }

  bool IsPercentageVisible() const {
    return view_->percentage_label_->visible();
  }

  bool IsTimeStatusVisible() const {
    return view_->time_status_label_->visible();
  }

  base::string16 RemainingTimeInView() const {
    return view_->time_status_label_->text();
  }

  gfx::ImageSkia GetBatteryImage() const { return view_->icon_->GetImage(); }

 private:
  std::unique_ptr<PowerStatusView> view_;

  DISALLOW_COPY_AND_ASSIGN(PowerStatusViewTest);
};

INSTANTIATE_TEST_CASE_P(
    /* prefix intentionally left blank due to only one parameterization */,
    PowerStatusViewTest,
    testing::Values(MaterialDesignController::NON_MATERIAL,
                    MaterialDesignController::MATERIAL_NORMAL,
                    MaterialDesignController::MATERIAL_EXPERIMENTAL));

TEST_P(PowerStatusViewTest, Basic) {
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
  EXPECT_NE(l10n_util::GetStringUTF16(
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
  EXPECT_NE(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_BATTERY_CHARGING_UNRELIABLE),
            RemainingTimeInView());

  prop.set_external_power(PowerSupplyProperties::USB);
  UpdatePowerStatus(prop);
  EXPECT_TRUE(IsPercentageVisible());
  EXPECT_TRUE(IsTimeStatusVisible());
  EXPECT_EQ(l10n_util::GetStringUTF16(
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

TEST_P(PowerStatusViewTest, AvoidNeedlessBatteryImageUpdates) {
  // No battery icon is shown in the material design system menu.
  if (ash::MaterialDesignController::UseMaterialDesignSystemIcons())
    return;

  PowerSupplyProperties prop;
  prop.set_external_power(PowerSupplyProperties::AC);
  prop.set_battery_state(PowerSupplyProperties::CHARGING);
  prop.set_battery_percent(50.0);
  UpdatePowerStatus(prop);

  // Create a copy of the view's ImageSkia (backed by the same bitmap). We hang
  // onto this to ensure that the original bitmap's memory doesn't get recycled
  // for a new bitmap, ensuring that we can safely compare bitmap addresses
  // later to check if the image that's being displayed has changed.
  const gfx::ImageSkia original_image = GetBatteryImage();

  // Send a no-op update. The old image should still be used.
  UpdatePowerStatus(prop);
  EXPECT_EQ(original_image.bitmap(), GetBatteryImage().bitmap());

  // Make a big change to the percentage and check that a new image is used.
  prop.set_battery_percent(100.0);
  UpdatePowerStatus(prop);
  EXPECT_NE(original_image.bitmap(), GetBatteryImage().bitmap());
}

}  // namespace ash
