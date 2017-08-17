// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/oobe_display_chooser.h"

#include <memory>
#include <vector>

#include "ash/display/display_configuration_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/display/manager/chromeos/touchscreen_util.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/touchscreen_device.h"

namespace chromeos {

namespace {

class OobeDisplayChooserTest : public ash::AshTestBase {
 public:
  OobeDisplayChooserTest() : ash::AshTestBase() {}

  int64_t GetPrimaryDisplay() {
    return display::Screen::GetScreen()->GetPrimaryDisplay().id();
  }

  void UpdateTouchscreenDevices(const ui::TouchscreenDevice& touchscreen) {
    std::vector<ui::TouchscreenDevice> devices{touchscreen};

    ui::DeviceHotplugEventObserver* manager =
        ui::DeviceDataManager::GetInstance();
    manager->OnTouchscreenDevicesUpdated(devices);
  }

  // ash::AshTestBase:
  void SetUp() override {
    ash::AshTestBase::SetUp();
    static_cast<ui::DeviceHotplugEventObserver*>(
        ui::DeviceDataManager::GetInstance())
        ->OnDeviceListsComplete();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OobeDisplayChooserTest);
};

const uint16_t kWhitelistedId = 0x266e;

}  // namespace

TEST_F(OobeDisplayChooserTest, PreferTouchAsPrimary) {
  // Setup 2 displays, second one is intended to be a touch display
  std::vector<display::ManagedDisplayInfo> display_info;
  display_info.push_back(
      display::ManagedDisplayInfo::CreateFromSpecWithID("0+0-3000x2000", 1));
  display_info.push_back(
      display::ManagedDisplayInfo::CreateFromSpecWithID("3000+0-800x600", 2));
  display_manager()->OnNativeDisplaysChanged(display_info);
  base::RunLoop().RunUntilIdle();

  // Make sure the non-touch display is primary
  ash::Shell::Get()->window_tree_host_manager()->SetPrimaryDisplayId(1);

  // Setup corresponding TouchscreenDevice object
  ui::TouchscreenDevice touchscreen =
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL,
                            "Touchscreen", gfx::Size(800, 600), 1);
  touchscreen.vendor_id = kWhitelistedId;
  UpdateTouchscreenDevices(touchscreen);
  base::RunLoop().RunUntilIdle();

  // Associate touchscreen device with display
  display_info[1].AddInputDevice(touchscreen.id);
  display_info[1].set_touch_support(display::Display::TOUCH_SUPPORT_AVAILABLE);
  display_manager()->OnNativeDisplaysChanged(display_info);
  base::RunLoop().RunUntilIdle();

  OobeDisplayChooser display_chooser;
  EXPECT_EQ(1, GetPrimaryDisplay());
  display_chooser.TryToPlaceUiOnTouchDisplay();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, GetPrimaryDisplay());
}

TEST_F(OobeDisplayChooserTest, DontSwitchFromTouch) {
  // Setup 2 displays, second one is intended to be a touch display
  std::vector<display::ManagedDisplayInfo> display_info;
  display_info.push_back(
      display::ManagedDisplayInfo::CreateFromSpecWithID("0+0-3000x2000", 1));
  display_info.push_back(
      display::ManagedDisplayInfo::CreateFromSpecWithID("3000+0-800x600", 2));
  display_info[0].set_touch_support(display::Display::TOUCH_SUPPORT_AVAILABLE);
  display_manager()->OnNativeDisplaysChanged(display_info);
  base::RunLoop().RunUntilIdle();

  // Make sure the non-touch display is primary
  ash::Shell::Get()->window_tree_host_manager()->SetPrimaryDisplayId(1);

  // Setup corresponding TouchscreenDevice object
  ui::TouchscreenDevice touchscreen =
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL,
                            "Touchscreen", gfx::Size(800, 600), 1);
  touchscreen.vendor_id = kWhitelistedId;
  UpdateTouchscreenDevices(touchscreen);
  base::RunLoop().RunUntilIdle();

  // Associate touchscreen device with display
  display_info[1].AddInputDevice(touchscreen.id);
  display_info[1].set_touch_support(display::Display::TOUCH_SUPPORT_AVAILABLE);
  display_manager()->OnNativeDisplaysChanged(display_info);
  base::RunLoop().RunUntilIdle();

  OobeDisplayChooser display_chooser;
  EXPECT_EQ(1, GetPrimaryDisplay());
  display_chooser.TryToPlaceUiOnTouchDisplay();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, GetPrimaryDisplay());
}

}  // namespace chromeos
