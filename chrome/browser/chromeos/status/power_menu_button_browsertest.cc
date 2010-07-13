// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/power_menu_button.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/browser/chromeos/cros/mock_power_library.h"
#include "chrome/browser/chromeos/frame/browser_view.h"
#include "chrome/browser/chromeos/status/browser_status_area_view.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "grit/theme_resources.h"

namespace chromeos {
using ::testing::AnyNumber;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;

class PowerMenuButtonTest : public CrosInProcessBrowserTest {
 protected:
  PowerMenuButtonTest() : CrosInProcessBrowserTest() {}

  virtual void SetUpInProcessBrowserTestFixture() {
    InitStatusAreaMocks();
    SetStatusAreaMocksExpectations();
  }

  PowerMenuButton* GetPowerMenuButton() {
    BrowserView* view = static_cast<BrowserView*>(browser()->window());
    PowerMenuButton* power = static_cast<BrowserStatusAreaView*>(view->
        GetViewByID(VIEW_ID_STATUS_AREA))->power_view();
    return power;
  }

  int CallPowerChangedAndGetIconId() {
    PowerMenuButton* power = GetPowerMenuButton();
    power->PowerChanged(mock_power_library_);
    return power->icon_id();
  }
};

IN_PROC_BROWSER_TEST_F(PowerMenuButtonTest, BatteryMissingTest) {
  EXPECT_CALL(*mock_power_library_, battery_is_present())
      .WillRepeatedly((Return(false)));
  EXPECT_EQ(IDR_STATUSBAR_BATTERY_MISSING, CallPowerChangedAndGetIconId());
}

IN_PROC_BROWSER_TEST_F(PowerMenuButtonTest, BatteryChargedTest) {
  EXPECT_CALL(*mock_power_library_, battery_is_present())
      .WillRepeatedly((Return(true)));
  EXPECT_CALL(*mock_power_library_, battery_fully_charged())
      .WillRepeatedly((Return(true)));
  EXPECT_CALL(*mock_power_library_, line_power_on())
      .WillRepeatedly((Return(true)));
  EXPECT_EQ(IDR_STATUSBAR_BATTERY_CHARGED, CallPowerChangedAndGetIconId());
}

// http://crbug.com/48912
IN_PROC_BROWSER_TEST_F(PowerMenuButtonTest, FAIL_BatteryChargingTest) {
  EXPECT_CALL(*mock_power_library_, battery_is_present())
      .WillRepeatedly((Return(true)));
  EXPECT_CALL(*mock_power_library_, battery_fully_charged())
      .WillRepeatedly((Return(false)));
  EXPECT_CALL(*mock_power_library_, line_power_on())
      .WillRepeatedly((Return(true)));

  // Test the 12 battery charging states.
  int id = IDR_STATUSBAR_BATTERY_CHARGING_1;
  for (float precent = 6.0; precent < 100.0; precent += 8.0) {
    EXPECT_CALL(*mock_power_library_, battery_percentage())
        .WillRepeatedly((Return(precent)));
    EXPECT_EQ(id, CallPowerChangedAndGetIconId());
    id++;
  }
}

// http://crbug.com/48912
IN_PROC_BROWSER_TEST_F(PowerMenuButtonTest, FAIL_BatteryDischargingTest) {
  EXPECT_CALL(*mock_power_library_, battery_is_present())
      .WillRepeatedly((Return(true)));
  EXPECT_CALL(*mock_power_library_, battery_fully_charged())
      .WillRepeatedly((Return(false)));
  EXPECT_CALL(*mock_power_library_, line_power_on())
      .WillRepeatedly((Return(false)));

  // Test the 12 battery discharing states.
  int id = IDR_STATUSBAR_BATTERY_DISCHARGING_1;
  for (float precent = 6.0; precent < 100.0; precent += 8.0) {
    EXPECT_CALL(*mock_power_library_, battery_percentage())
        .WillRepeatedly((Return(precent)));
    EXPECT_EQ(id, CallPowerChangedAndGetIconId());
    id++;
  }
}

}  // namespace chromeos
