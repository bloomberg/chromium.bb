// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/power_menu_button.h"

#include "chrome/browser/chromeos/frame/browser_view.h"
#include "chrome/browser/chromeos/status/status_area_view.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/views/view.h"

#if defined(USE_AURA)
#include "chrome/browser/ui/views/aura/chrome_shell_delegate.h"
#endif

namespace chromeos {

class PowerMenuButtonTest : public InProcessBrowserTest {
 protected:
  PowerMenuButtonTest() : InProcessBrowserTest() {
  }

  virtual void SetUpInProcessBrowserTestFixture() {
  }

  PowerMenuButton* GetPowerMenuButton() {
    views::View* view =
#if defined(USE_AURA)
        ChromeShellDelegate::instance()->GetStatusArea();
#else
        static_cast<BrowserView*>(browser()->window());
#endif
    return static_cast<PowerMenuButton*>(
        view->GetViewByID(VIEW_ID_STATUS_BUTTON_POWER));
  }

  string16 CallPowerChangedAndGetTooltipText(const PowerSupplyStatus& status) {
    PowerMenuButton* power = GetPowerMenuButton();

    power->PowerChanged(status);
    EXPECT_TRUE(power->IsVisible());

    string16 tooltip;
    // There is static_cast<StatusAreaButton*> because GetTootipText is also
    // declared in MenuDelegate
    EXPECT_TRUE(static_cast<StatusAreaButton*>(power)->GetTooltipText(
        gfx::Point(0, 0), &tooltip));
    return tooltip;
  }

  string16 SetBatteryPercentageTo50AndGetTooltipText() {
    PowerSupplyStatus status;
    // No battery present.
    status.battery_is_present    = true;
    status.battery_percentage    = 50.0;
    status.battery_is_full       = false;
    status.line_power_on         = false;
    status.battery_seconds_to_empty = 42;
    status.battery_seconds_to_full  = 24;
    return CallPowerChangedAndGetTooltipText(status);
  }
};

IN_PROC_BROWSER_TEST_F(PowerMenuButtonTest, BatteryMissingTest) {
  const string16 tooltip_before = SetBatteryPercentageTo50AndGetTooltipText();

  PowerSupplyStatus status;
  // No battery present.
  status.battery_is_present    = false;
  status.battery_percentage    = 42.0;
  status.battery_is_full       = false;
  status.line_power_on         = true;
  status.battery_seconds_to_empty = 42;
  status.battery_seconds_to_full  = 24;

  PowerMenuButton* power = GetPowerMenuButton();
  power->PowerChanged(status);

  EXPECT_FALSE(power->IsVisible());
}

IN_PROC_BROWSER_TEST_F(PowerMenuButtonTest, BatteryNotSupportedTest) {
  PowerSupplyStatus status;
  // No battery present.
  status.battery_is_present    = false;
  status.battery_percentage    = 42.0;
  status.battery_is_full       = false;
  status.line_power_on         = false;
  status.battery_seconds_to_empty = 42;
  status.battery_seconds_to_full  = 24;

  PowerMenuButton* power = GetPowerMenuButton();
  power->PowerChanged(status);

  EXPECT_FALSE(power->IsVisible());
}

IN_PROC_BROWSER_TEST_F(PowerMenuButtonTest, BatteryChargedTest) {
  const string16 tooltip_before = SetBatteryPercentageTo50AndGetTooltipText();

  PowerSupplyStatus status;
  // The battery is fully charged, and line power is plugged in.
  status.battery_is_present    = true;
  status.battery_percentage    = 42.0;
  status.battery_is_full       = true;
  status.line_power_on         = true;
  status.battery_seconds_to_empty = 42;
  status.battery_seconds_to_full  = 24;

  EXPECT_NE(tooltip_before, CallPowerChangedAndGetTooltipText(status));
}

IN_PROC_BROWSER_TEST_F(PowerMenuButtonTest, BatteryChargingTest) {
  string16 tooltip_before = SetBatteryPercentageTo50AndGetTooltipText();

  PowerSupplyStatus status;
  status.battery_is_present    = true;
  status.battery_is_full       = false;
  status.line_power_on         = true;
  status.battery_seconds_to_empty = 42;
  status.battery_seconds_to_full  = 24;

  // Simulate various levels of charging.
  for (double percent = 5.0; percent < 100.0; percent += 5.0) {
    status.battery_percentage = percent;
    const string16 tooltip_after = CallPowerChangedAndGetTooltipText(status);
    EXPECT_NE(tooltip_before, tooltip_after);
    tooltip_before = tooltip_after;
  }
}

IN_PROC_BROWSER_TEST_F(PowerMenuButtonTest, BatteryDischargingTest) {
  string16 tooltip_before = SetBatteryPercentageTo50AndGetTooltipText();

  PowerSupplyStatus status;
  status.battery_is_present    = true;
  status.battery_is_full       = false;
  status.line_power_on         = false;
  status.battery_seconds_to_empty = 42;
  status.battery_seconds_to_full  = 24;

  // Simulate various levels of discharging.
  for (double percent = 5.0; percent < 100.0; percent += 5.0) {
    status.battery_percentage = percent;
    const string16 tooltip_after = CallPowerChangedAndGetTooltipText(status);
    EXPECT_NE(tooltip_before, tooltip_after);
    tooltip_before = tooltip_after;
  }
}

}  // namespace chromeos
