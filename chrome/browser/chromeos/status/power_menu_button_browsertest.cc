// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/power_menu_button.h"

#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/browser/chromeos/cros/mock_power_library.h"
#include "chrome/browser/chromeos/frame/browser_view.h"
#include "chrome/browser/chromeos/status/status_area_view.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "grit/theme_resources.h"

namespace {
const int kNumBatteryStates = 20;
}
namespace chromeos {
using ::testing::AnyNumber;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;

class PowerMenuButtonTest : public CrosInProcessBrowserTest {
 protected:
  PowerMenuButtonTest() : CrosInProcessBrowserTest() {
  }

  virtual void SetUpInProcessBrowserTestFixture() {
    cros_mock_->InitStatusAreaMocks();
    cros_mock_->SetStatusAreaMocksExpectations();
  }

  PowerMenuButton* GetPowerMenuButton() {
    BrowserView* view = static_cast<BrowserView*>(browser()->window());
    PowerMenuButton* power = static_cast<StatusAreaView*>(view->
        GetViewByID(VIEW_ID_STATUS_AREA))->power_view();
    return power;
  }

  int CallPowerChangedAndGetBatteryIndex(const PowerSupplyStatus& status) {
    PowerMenuButton* power = GetPowerMenuButton();
    power->PowerChanged(status);
    return power->battery_index();
  }
};

IN_PROC_BROWSER_TEST_F(PowerMenuButtonTest, BatteryMissingTest) {
  PowerSupplyStatus status;
  // No battery present.
  status.battery_is_present    = false;
  status.battery_percentage    = 42.0;
  status.battery_is_full       = false;
  status.line_power_on         = false;
  status.battery_seconds_to_empty = 42;
  status.battery_seconds_to_full  = 24;

  EXPECT_EQ(-1, CallPowerChangedAndGetBatteryIndex(status));
}

IN_PROC_BROWSER_TEST_F(PowerMenuButtonTest, BatteryChargedTest) {
  PowerSupplyStatus status;
  // The battery is fully charged, and line power is plugged in.
  status.battery_is_present    = true;
  status.battery_percentage    = 42.0;
  status.battery_is_full       = true;
  status.line_power_on         = true;
  status.battery_seconds_to_empty = 42;
  status.battery_seconds_to_full  = 24;

  EXPECT_EQ(kNumBatteryStates - 1, CallPowerChangedAndGetBatteryIndex(status));
}

IN_PROC_BROWSER_TEST_F(PowerMenuButtonTest, BatteryChargingTest) {
  PowerSupplyStatus status;
  status.battery_is_present    = true;
  status.battery_is_full       = false;
  status.line_power_on         = true;
  status.battery_seconds_to_empty = 42;
  status.battery_seconds_to_full  = 24;

  // Simulate various levels of charging.
  int index = 0;
  for (double percent = 5.0; percent < 100.0; percent += 5.0) {
    status.battery_percentage = percent;
    EXPECT_EQ(index, CallPowerChangedAndGetBatteryIndex(status));
    index++;
  }
}

IN_PROC_BROWSER_TEST_F(PowerMenuButtonTest, BatteryDischargingTest) {
  PowerSupplyStatus status;
  status.battery_is_present    = true;
  status.battery_is_full       = false;
  status.line_power_on         = false;
  status.battery_seconds_to_empty = 42;
  status.battery_seconds_to_full  = 24;

  // Simulate various levels of discharging.
  int index = 0;
  for (float percent = 5.0; percent < 100.0; percent += 5.0) {
    status.battery_percentage = percent;
    EXPECT_EQ(index, CallPowerChangedAndGetBatteryIndex(status));
    index++;
  }
}

}  // namespace chromeos
