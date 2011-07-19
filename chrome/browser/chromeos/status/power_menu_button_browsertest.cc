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

namespace chromeos {
using ::testing::AnyNumber;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;

class PowerMenuButtonTest : public CrosInProcessBrowserTest {
 protected:
  MockPowerLibrary *mock_power_library_;

  PowerMenuButtonTest() : CrosInProcessBrowserTest(),
                          mock_power_library_(NULL) {
  }

  virtual void SetUpInProcessBrowserTestFixture() {
    cros_mock_->InitStatusAreaMocks();
    cros_mock_->SetStatusAreaMocksExpectations();
    mock_power_library_ = cros_mock_->mock_power_library();
  }

  PowerMenuButton* GetPowerMenuButton() {
    BrowserView* view = static_cast<BrowserView*>(browser()->window());
    PowerMenuButton* power = static_cast<StatusAreaView*>(view->
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
      .WillOnce((Return(false)))  // no battery
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, battery_percentage())
      .WillOnce((Return(42.0)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, battery_fully_charged())
      .WillOnce((Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, line_power_on())
      .WillOnce((Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, battery_time_to_empty())
      .WillOnce((Return(base::TimeDelta::FromMinutes(42))))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, battery_time_to_full())
      .WillOnce((Return(base::TimeDelta::FromMinutes(24))))
      .RetiresOnSaturation();
  EXPECT_EQ(IDR_STATUSBAR_BATTERY_MISSING, CallPowerChangedAndGetIconId());
}

IN_PROC_BROWSER_TEST_F(PowerMenuButtonTest, BatteryChargedTest) {
  EXPECT_CALL(*mock_power_library_, battery_is_present())
      .WillOnce((Return(true)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, battery_percentage())
      .WillOnce((Return(42.0)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, battery_fully_charged())
      .WillOnce((Return(true)))  // fully charged
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, line_power_on())
      .WillOnce((Return(true)))  // plugged in
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, battery_time_to_empty())
      .WillOnce((Return(base::TimeDelta::FromMinutes(42))))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, battery_time_to_full())
      .WillOnce((Return(base::TimeDelta::FromMinutes(0))))
      .RetiresOnSaturation();
  EXPECT_EQ(IDR_STATUSBAR_BATTERY_CHARGED, CallPowerChangedAndGetIconId());
}

IN_PROC_BROWSER_TEST_F(PowerMenuButtonTest, BatteryChargingTest) {
  const int NUM_TIMES = 19;
  EXPECT_CALL(*mock_power_library_, battery_is_present())
      .Times(NUM_TIMES)
      .WillRepeatedly((Return(true)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, battery_fully_charged())
      .Times(NUM_TIMES)
      .WillRepeatedly((Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, line_power_on())
      .Times(NUM_TIMES)
      .WillRepeatedly((Return(true)))  // plugged in
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, battery_time_to_empty())
      .Times(NUM_TIMES)
      .WillRepeatedly((Return(base::TimeDelta::FromMinutes(42))))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, battery_time_to_full())
      .Times(NUM_TIMES)
      .WillRepeatedly((Return(base::TimeDelta::FromMinutes(24))))
      .RetiresOnSaturation();

  // Test the 16 battery charging states.
  // NOTE: Use an array rather than just calculating a resource number to avoid
  // creating implicit ordering dependencies on the resource values.
  static const int kChargingImages[] = {
    IDR_STATUSBAR_BATTERY_CHARGING_1,
    IDR_STATUSBAR_BATTERY_CHARGING_2,
    IDR_STATUSBAR_BATTERY_CHARGING_3,
    IDR_STATUSBAR_BATTERY_CHARGING_4,
    IDR_STATUSBAR_BATTERY_CHARGING_5,
    IDR_STATUSBAR_BATTERY_CHARGING_6,
    IDR_STATUSBAR_BATTERY_CHARGING_7,
    IDR_STATUSBAR_BATTERY_CHARGING_8,
    IDR_STATUSBAR_BATTERY_CHARGING_9,
    IDR_STATUSBAR_BATTERY_CHARGING_10,
    IDR_STATUSBAR_BATTERY_CHARGING_11,
    IDR_STATUSBAR_BATTERY_CHARGING_12,
    IDR_STATUSBAR_BATTERY_CHARGING_13,
    IDR_STATUSBAR_BATTERY_CHARGING_14,
    IDR_STATUSBAR_BATTERY_CHARGING_15,
    IDR_STATUSBAR_BATTERY_CHARGING_16,
    IDR_STATUSBAR_BATTERY_CHARGING_17,
    IDR_STATUSBAR_BATTERY_CHARGING_18,
    IDR_STATUSBAR_BATTERY_CHARGING_19,
  };
  size_t id = 0;
  for (float percent = 5.0; percent < 100.0; percent += 5.0) {
    EXPECT_CALL(*mock_power_library_, battery_percentage())
        .WillOnce((Return(percent)))
        .RetiresOnSaturation();
    ASSERT_LT(id, arraysize(kChargingImages));
    EXPECT_EQ(kChargingImages[id], CallPowerChangedAndGetIconId());
    id++;
  }
}

IN_PROC_BROWSER_TEST_F(PowerMenuButtonTest, BatteryDischargingTest) {
  const int NUM_TIMES = 19;
  EXPECT_CALL(*mock_power_library_, battery_is_present())
      .Times(NUM_TIMES)
      .WillRepeatedly((Return(true)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, battery_fully_charged())
      .Times(NUM_TIMES)
      .WillRepeatedly((Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, line_power_on())
      .Times(NUM_TIMES)
      .WillRepeatedly((Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, battery_time_to_empty())
      .Times(NUM_TIMES)
      .WillRepeatedly((Return(base::TimeDelta::FromMinutes(42))))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, battery_time_to_full())
      .Times(NUM_TIMES)
      .WillRepeatedly((Return(base::TimeDelta::FromMinutes(24))))
      .RetiresOnSaturation();

  // Test the 16 battery discharing states.
  // NOTE: Use an array rather than just calculating a resource number to avoid
  // creating implicit ordering dependencies on the resource values.
  static const int kDischargingImages[] = {
    IDR_STATUSBAR_BATTERY_DISCHARGING_1,
    IDR_STATUSBAR_BATTERY_DISCHARGING_2,
    IDR_STATUSBAR_BATTERY_DISCHARGING_3,
    IDR_STATUSBAR_BATTERY_DISCHARGING_4,
    IDR_STATUSBAR_BATTERY_DISCHARGING_5,
    IDR_STATUSBAR_BATTERY_DISCHARGING_6,
    IDR_STATUSBAR_BATTERY_DISCHARGING_7,
    IDR_STATUSBAR_BATTERY_DISCHARGING_8,
    IDR_STATUSBAR_BATTERY_DISCHARGING_9,
    IDR_STATUSBAR_BATTERY_DISCHARGING_10,
    IDR_STATUSBAR_BATTERY_DISCHARGING_11,
    IDR_STATUSBAR_BATTERY_DISCHARGING_12,
    IDR_STATUSBAR_BATTERY_DISCHARGING_13,
    IDR_STATUSBAR_BATTERY_DISCHARGING_14,
    IDR_STATUSBAR_BATTERY_DISCHARGING_15,
    IDR_STATUSBAR_BATTERY_DISCHARGING_16,
    IDR_STATUSBAR_BATTERY_DISCHARGING_17,
    IDR_STATUSBAR_BATTERY_DISCHARGING_18,
    IDR_STATUSBAR_BATTERY_DISCHARGING_19,
  };
  size_t id = 0;
  for (float percent = 5.0; percent < 100.0; percent += 5.0) {
    EXPECT_CALL(*mock_power_library_, battery_percentage())
        .WillOnce((Return(percent)))
        .RetiresOnSaturation();
    ASSERT_LT(id, arraysize(kDischargingImages));
    EXPECT_EQ(kDischargingImages[id], CallPowerChangedAndGetIconId());
    id++;
  }
}

}  // namespace chromeos
