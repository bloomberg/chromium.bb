// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_display/display_info_provider.h"

#include <stdint.h>

#include "ash/display/screen_orientation_controller_chromeos.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/touch/ash_touch_transform_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/display_info_provider_chromeos.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "extensions/common/api/system_display.h"
#include "services/ui/public/cpp/input_devices/input_device_client_test_api.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/chromeos/test/touch_transform_controller_test_api.h"
#include "ui/display/manager/chromeos/touch_transform_setter.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/devices/touch_device_transform.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/gfx/geometry/rect.h"

namespace extensions {
namespace {

using DisplayUnitInfoList = DisplayInfoProvider::DisplayUnitInfoList;
using DisplayLayoutList = DisplayInfoProvider::DisplayLayoutList;

void InitExternalTouchDevices(int64_t display_id) {
  ui::TouchscreenDevice touchdevice(
      123, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL,
      std::string("test external touch device"), gfx::Size(1000, 1000), 1);
  ui::InputDeviceClientTestApi().SetTouchscreenDevices({touchdevice});

  std::vector<ui::TouchDeviceTransform> transforms;
  ui::TouchDeviceTransform touch_device_transform;
  touch_device_transform.display_id = display_id;
  touch_device_transform.device_id = touchdevice.id;
  transforms.push_back(touch_device_transform);
  display::test::TouchTransformControllerTestApi(
      ash::Shell::Get()->touch_transformer_controller())
      .touch_transform_setter()
      ->ConfigureTouchDevices(transforms);
}

class DisplayInfoProviderChromeosTest : public ash::AshTestBase {
 public:
  DisplayInfoProviderChromeosTest() {}

  ~DisplayInfoProviderChromeosTest() override {}

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFirstDisplayAsInternal);

    ash::AshTestBase::SetUp();

    tablet_mode_client_ = std::make_unique<TabletModeClient>();
    ash::mojom::TabletModeControllerPtr controller;
    ash::Shell::Get()->tablet_mode_controller()->BindRequest(
        mojo::MakeRequest(&controller));
    tablet_mode_client_->InitForTesting(std::move(controller));
    // We must flush the TabletModeClient as we are waiting for the initial
    // value to be set, as the TabletModeController sends an initial message on
    // startup when it observes the PowerManagerClient.
    tablet_mode_client_->FlushForTesting();
  }

 protected:
  void CallSetDisplayUnitInfo(
      const std::string& display_id,
      const api::system_display::DisplayProperties& info,
      bool* success,
      std::string* error) {
    // Reset error messsage.
    (*error).clear();
    *success = DisplayInfoProvider::Get()->SetInfo(display_id, info, error);
  }

  bool DisplayExists(int64_t display_id) const {
    const display::Display& display =
        GetDisplayManager()->GetDisplayForId(display_id);
    return display.id() != display::kInvalidDisplayId;
  }

  void EnableTabletMode(bool enable) {
    ash::TabletModeController* controller =
        ash::Shell::Get()->tablet_mode_controller();
    controller->EnableTabletModeWindowManager(enable);
    tablet_mode_client_->FlushForTesting();
  }

  display::DisplayManager* GetDisplayManager() const {
    return ash::Shell::Get()->display_manager();
  }

  std::string SystemInfoDisplayInsetsToString(
      const api::system_display::Insets& insets) const {
    // Order to match gfx::Insets::ToString().
    return base::StringPrintf(
        "%d,%d,%d,%d", insets.top, insets.left, insets.bottom, insets.right);
  }

  std::string SystemInfoDisplayBoundsToString(
      const api::system_display::Bounds& bounds) const {
    // Order to match gfx::Rect::ToString().
    return base::StringPrintf(
        "%d,%d %dx%d", bounds.left, bounds.top, bounds.width, bounds.height);
  }

  DisplayUnitInfoList GetAllDisplaysInfo() {
    return DisplayInfoProvider::Get()->GetAllDisplaysInfo(
        false /* single unified */);
  }

 private:
  std::unique_ptr<TabletModeClient> tablet_mode_client_;

  DISALLOW_COPY_AND_ASSIGN(DisplayInfoProviderChromeosTest);
};

TEST_F(DisplayInfoProviderChromeosTest, GetBasic) {
  UpdateDisplay("500x600,400x520");
  DisplayUnitInfoList result = GetAllDisplaysInfo();

  ASSERT_EQ(2u, result.size());

  int64_t display_id;
  ASSERT_TRUE(base::StringToInt64(result[0].id, &display_id))
      << "Display id must be convertable to integer: " << result[0].id;

  ASSERT_TRUE(DisplayExists(display_id)) << display_id << " not found";
  EXPECT_EQ("0,0 500x600", SystemInfoDisplayBoundsToString(result[0].bounds));
  EXPECT_EQ("0,0,0,0", SystemInfoDisplayInsetsToString(result[0].overscan));
  EXPECT_EQ(0, result[0].rotation);
  EXPECT_TRUE(result[0].is_primary);
  EXPECT_EQ(96, result[0].dpi_x);
  EXPECT_EQ(96, result[0].dpi_y);
  EXPECT_TRUE(result[0].mirroring_source_id.empty());
  EXPECT_TRUE(result[0].is_enabled);

  ASSERT_TRUE(base::StringToInt64(result[1].id, &display_id))
      << "Display id must be convertable to integer: " << result[0].id;

  ASSERT_TRUE(DisplayExists(display_id)) << display_id << " not found";
  EXPECT_EQ(GetDisplayManager()->GetDisplayNameForId(display_id),
            result[1].name);
  // The second display is positioned left of the primary display, whose width
  // is 500.
  EXPECT_EQ("500,0 400x520", SystemInfoDisplayBoundsToString(result[1].bounds));
  EXPECT_EQ("0,0,0,0", SystemInfoDisplayInsetsToString(result[1].overscan));
  EXPECT_EQ(0, result[1].rotation);
  EXPECT_FALSE(result[1].is_primary);
  EXPECT_EQ(96, result[1].dpi_x);
  EXPECT_EQ(96, result[1].dpi_y);
  EXPECT_TRUE(result[1].mirroring_source_id.empty());
  EXPECT_TRUE(result[1].is_enabled);
}

TEST_F(DisplayInfoProviderChromeosTest, GetWithUnifiedDesktop) {
  UpdateDisplay("500x600,400x520");

  // Check initial state.
  EXPECT_FALSE(GetDisplayManager()->IsInUnifiedMode());
  DisplayUnitInfoList result = GetAllDisplaysInfo();

  ASSERT_EQ(2u, result.size());

  int64_t display_id;
  ASSERT_TRUE(base::StringToInt64(result[0].id, &display_id))
      << "Display id must be convertable to integer: " << result[0].id;

  ASSERT_TRUE(DisplayExists(display_id)) << display_id << " not found";
  EXPECT_EQ("0,0 500x600", SystemInfoDisplayBoundsToString(result[0].bounds));
  EXPECT_EQ("0,0,0,0", SystemInfoDisplayInsetsToString(result[0].overscan));
  EXPECT_EQ(0, result[0].rotation);
  EXPECT_TRUE(result[0].is_primary);
  EXPECT_FALSE(result[0].is_unified);
  EXPECT_EQ(96, result[0].dpi_x);
  EXPECT_EQ(96, result[0].dpi_y);
  EXPECT_TRUE(result[0].mirroring_source_id.empty());
  EXPECT_TRUE(result[0].is_enabled);

  ASSERT_TRUE(base::StringToInt64(result[1].id, &display_id))
      << "Display id must be convertable to integer: " << result[0].id;

  ASSERT_TRUE(DisplayExists(display_id)) << display_id << " not found";
  EXPECT_EQ(GetDisplayManager()->GetDisplayNameForId(display_id),
            result[1].name);

  // Initial multipple display configuration.
  EXPECT_EQ("500,0 400x520", SystemInfoDisplayBoundsToString(result[1].bounds));
  EXPECT_EQ("0,0,0,0", SystemInfoDisplayInsetsToString(result[1].overscan));
  EXPECT_EQ(0, result[1].rotation);
  EXPECT_FALSE(result[1].is_primary);
  EXPECT_FALSE(result[0].is_unified);
  EXPECT_EQ(96, result[1].dpi_x);
  EXPECT_EQ(96, result[1].dpi_y);
  EXPECT_TRUE(result[1].mirroring_source_id.empty());
  EXPECT_TRUE(result[1].is_enabled);

  // Enable unified.
  GetDisplayManager()->SetUnifiedDesktopEnabled(true);
  EXPECT_TRUE(GetDisplayManager()->IsInUnifiedMode());

  result = GetAllDisplaysInfo();

  ASSERT_EQ(2u, result.size());

  ASSERT_TRUE(base::StringToInt64(result[0].id, &display_id))
      << "Display id must be convertable to integer: " << result[0].id;

  EXPECT_EQ("0,0 500x600", SystemInfoDisplayBoundsToString(result[0].bounds));
  EXPECT_EQ("0,0,0,0", SystemInfoDisplayInsetsToString(result[0].overscan));
  EXPECT_EQ(0, result[0].rotation);
  EXPECT_TRUE(result[0].is_primary);
  EXPECT_TRUE(result[0].is_unified);
  EXPECT_EQ(96, result[0].dpi_x);
  EXPECT_EQ(96, result[0].dpi_y);
  EXPECT_TRUE(result[0].mirroring_source_id.empty());
  EXPECT_TRUE(result[0].is_enabled);

  ASSERT_TRUE(base::StringToInt64(result[1].id, &display_id))
      << "Display id must be convertable to integer: " << result[0].id;

  EXPECT_EQ(GetDisplayManager()->GetDisplayNameForId(display_id),
            result[1].name);

  // After enabling unified the second display is scaled to meet the height for
  // the first. Which also affects the DPI below.
  EXPECT_EQ("500,0 461x600", SystemInfoDisplayBoundsToString(result[1].bounds));
  EXPECT_EQ("0,0,0,0", SystemInfoDisplayInsetsToString(result[1].overscan));
  EXPECT_EQ(0, result[1].rotation);
  EXPECT_FALSE(result[1].is_primary);
  EXPECT_TRUE(result[1].is_unified);
  EXPECT_FLOAT_EQ(111, round(result[1].dpi_x));
  EXPECT_FLOAT_EQ(111, round(result[1].dpi_y));
  EXPECT_TRUE(result[1].mirroring_source_id.empty());
  EXPECT_TRUE(result[1].is_enabled);

  // Disable unified and check that once again it matches initial situation.
  GetDisplayManager()->SetUnifiedDesktopEnabled(false);
  EXPECT_FALSE(GetDisplayManager()->IsInUnifiedMode());
  result = GetAllDisplaysInfo();

  ASSERT_EQ(2u, result.size());

  ASSERT_TRUE(base::StringToInt64(result[0].id, &display_id))
      << "Display id must be convertable to integer: " << result[0].id;

  ASSERT_TRUE(DisplayExists(display_id)) << display_id << " not found";
  EXPECT_EQ("0,0 500x600", SystemInfoDisplayBoundsToString(result[0].bounds));
  EXPECT_EQ("0,0,0,0", SystemInfoDisplayInsetsToString(result[0].overscan));
  EXPECT_EQ(0, result[0].rotation);
  EXPECT_TRUE(result[0].is_primary);
  EXPECT_FALSE(result[0].is_unified);
  EXPECT_EQ(96, result[0].dpi_x);
  EXPECT_EQ(96, result[0].dpi_y);
  EXPECT_TRUE(result[0].mirroring_source_id.empty());
  EXPECT_TRUE(result[0].is_enabled);

  ASSERT_TRUE(base::StringToInt64(result[1].id, &display_id))
      << "Display id must be convertable to integer: " << result[0].id;

  ASSERT_TRUE(DisplayExists(display_id)) << display_id << " not found";
  EXPECT_EQ(GetDisplayManager()->GetDisplayNameForId(display_id),
            result[1].name);
  EXPECT_EQ("500,0 400x520", SystemInfoDisplayBoundsToString(result[1].bounds));
  EXPECT_EQ("0,0,0,0", SystemInfoDisplayInsetsToString(result[1].overscan));
  EXPECT_EQ(0, result[1].rotation);
  EXPECT_FALSE(result[1].is_primary);
  EXPECT_FALSE(result[1].is_unified);
  EXPECT_EQ(96, result[1].dpi_x);
  EXPECT_EQ(96, result[1].dpi_y);
  EXPECT_TRUE(result[1].mirroring_source_id.empty());
  EXPECT_TRUE(result[1].is_enabled);
}

TEST_F(DisplayInfoProviderChromeosTest, GetWithUnifiedDesktopForSettings) {
  UpdateDisplay("500x600,400x520");

  // Check initial state.
  EXPECT_FALSE(GetDisplayManager()->IsInUnifiedMode());
  DisplayUnitInfoList result =
      DisplayInfoProvider::Get()->GetAllDisplaysInfo(true /* single unifed */);

  ASSERT_EQ(2u, result.size());

  int64_t display_id;
  ASSERT_TRUE(base::StringToInt64(result[0].id, &display_id))
      << "Display id must be convertable to integer: " << result[0].id;

  ASSERT_TRUE(DisplayExists(display_id)) << display_id << " not found";
  EXPECT_EQ("0,0 500x600", SystemInfoDisplayBoundsToString(result[0].bounds));
  EXPECT_EQ("0,0,0,0", SystemInfoDisplayInsetsToString(result[0].overscan));
  EXPECT_EQ(0, result[0].rotation);
  EXPECT_TRUE(result[0].is_primary);
  EXPECT_FALSE(result[0].is_unified);
  EXPECT_EQ(96, result[0].dpi_x);
  EXPECT_EQ(96, result[0].dpi_y);
  EXPECT_TRUE(result[0].mirroring_source_id.empty());
  EXPECT_TRUE(result[0].is_enabled);

  ASSERT_TRUE(base::StringToInt64(result[1].id, &display_id))
      << "Display id must be convertable to integer: " << result[0].id;

  ASSERT_TRUE(DisplayExists(display_id)) << display_id << " not found";
  EXPECT_EQ(GetDisplayManager()->GetDisplayNameForId(display_id),
            result[1].name);

  // Initial multipple display configuration.
  EXPECT_EQ("500,0 400x520", SystemInfoDisplayBoundsToString(result[1].bounds));
  EXPECT_EQ("0,0,0,0", SystemInfoDisplayInsetsToString(result[1].overscan));
  EXPECT_EQ(0, result[1].rotation);
  EXPECT_FALSE(result[1].is_primary);
  EXPECT_FALSE(result[0].is_unified);
  EXPECT_EQ(96, result[1].dpi_x);
  EXPECT_EQ(96, result[1].dpi_y);
  EXPECT_TRUE(result[1].mirroring_source_id.empty());
  EXPECT_TRUE(result[1].is_enabled);

  // Enable unified.
  GetDisplayManager()->SetUnifiedDesktopEnabled(true);
  EXPECT_TRUE(GetDisplayManager()->IsInUnifiedMode());

  // For settings, GetAllDisplaysInfo will return a single unified display. The
  // second display will be scaled to match the height of the first, so the
  // height will be 600 and the new width will be 500 + [400 * 600/520 = 461] =
  // 961.
  result =
      DisplayInfoProvider::Get()->GetAllDisplaysInfo(true /* single unified */);

  ASSERT_EQ(1u, result.size());
  ASSERT_TRUE(base::StringToInt64(result[0].id, &display_id))
      << "Display id must be convertable to integer: " << result[0].id;

  EXPECT_EQ("0,0 961x600", SystemInfoDisplayBoundsToString(result[0].bounds));
  EXPECT_EQ("0,0,0,0", SystemInfoDisplayInsetsToString(result[0].overscan));
  EXPECT_EQ(0, result[0].rotation);
  EXPECT_TRUE(result[0].is_primary);
  EXPECT_TRUE(result[0].is_unified);
  EXPECT_EQ(96, result[0].dpi_x);
  EXPECT_EQ(96, result[0].dpi_y);
  EXPECT_TRUE(result[0].mirroring_source_id.empty());
  EXPECT_TRUE(result[0].is_enabled);

  // Disable unified and check that once again it matches initial situation.
  GetDisplayManager()->SetUnifiedDesktopEnabled(false);
  EXPECT_FALSE(GetDisplayManager()->IsInUnifiedMode());
  result = GetAllDisplaysInfo();

  ASSERT_EQ(2u, result.size());

  ASSERT_TRUE(base::StringToInt64(result[0].id, &display_id))
      << "Display id must be convertable to integer: " << result[0].id;

  ASSERT_TRUE(DisplayExists(display_id)) << display_id << " not found";
  EXPECT_EQ("0,0 500x600", SystemInfoDisplayBoundsToString(result[0].bounds));
  EXPECT_EQ("0,0,0,0", SystemInfoDisplayInsetsToString(result[0].overscan));
  EXPECT_EQ(0, result[0].rotation);
  EXPECT_TRUE(result[0].is_primary);
  EXPECT_FALSE(result[0].is_unified);
  EXPECT_EQ(96, result[0].dpi_x);
  EXPECT_EQ(96, result[0].dpi_y);
  EXPECT_TRUE(result[0].mirroring_source_id.empty());
  EXPECT_TRUE(result[0].is_enabled);

  ASSERT_TRUE(base::StringToInt64(result[1].id, &display_id))
      << "Display id must be convertable to integer: " << result[0].id;

  ASSERT_TRUE(DisplayExists(display_id)) << display_id << " not found";
  EXPECT_EQ(GetDisplayManager()->GetDisplayNameForId(display_id),
            result[1].name);
  EXPECT_EQ("500,0 400x520", SystemInfoDisplayBoundsToString(result[1].bounds));
  EXPECT_EQ("0,0,0,0", SystemInfoDisplayInsetsToString(result[1].overscan));
  EXPECT_EQ(0, result[1].rotation);
  EXPECT_FALSE(result[1].is_primary);
  EXPECT_FALSE(result[1].is_unified);
  EXPECT_EQ(96, result[1].dpi_x);
  EXPECT_EQ(96, result[1].dpi_y);
  EXPECT_TRUE(result[1].mirroring_source_id.empty());
  EXPECT_TRUE(result[1].is_enabled);
}

TEST_F(DisplayInfoProviderChromeosTest, GetRotation) {
  UpdateDisplay("500x600/r");
  DisplayUnitInfoList result = GetAllDisplaysInfo();

  ASSERT_EQ(1u, result.size());

  int64_t display_id;
  ASSERT_TRUE(base::StringToInt64(result[0].id, &display_id))
      << "Display id must be convertable to integer: " << result[0].id;

  ASSERT_TRUE(DisplayExists(display_id)) << display_id << " not found";
  EXPECT_EQ("0,0 600x500", SystemInfoDisplayBoundsToString(result[0].bounds));
  EXPECT_EQ(90, result[0].rotation);

  GetDisplayManager()->SetDisplayRotation(
      display_id, display::Display::ROTATE_270,
      display::Display::RotationSource::ACTIVE);

  result = GetAllDisplaysInfo();

  ASSERT_EQ(1u, result.size());

  EXPECT_EQ(base::Int64ToString(display_id), result[0].id);
  EXPECT_EQ("0,0 600x500", SystemInfoDisplayBoundsToString(result[0].bounds));
  EXPECT_EQ(270, result[0].rotation);

  GetDisplayManager()->SetDisplayRotation(
      display_id, display::Display::ROTATE_180,
      display::Display::RotationSource::ACTIVE);

  result = GetAllDisplaysInfo();

  ASSERT_EQ(1u, result.size());

  EXPECT_EQ(base::Int64ToString(display_id), result[0].id);
  EXPECT_EQ("0,0 500x600", SystemInfoDisplayBoundsToString(result[0].bounds));
  EXPECT_EQ(180, result[0].rotation);

  GetDisplayManager()->SetDisplayRotation(
      display_id, display::Display::ROTATE_0,
      display::Display::RotationSource::ACTIVE);

  result = GetAllDisplaysInfo();

  ASSERT_EQ(1u, result.size());

  EXPECT_EQ(base::Int64ToString(display_id), result[0].id);
  EXPECT_EQ("0,0 500x600", SystemInfoDisplayBoundsToString(result[0].bounds));
  EXPECT_EQ(0, result[0].rotation);
}

TEST_F(DisplayInfoProviderChromeosTest, GetDPI) {
  UpdateDisplay("500x600,400x520*2");
  DisplayUnitInfoList result;
  result = GetAllDisplaysInfo();

  ASSERT_EQ(2u, result.size());

  EXPECT_EQ("0,0 500x600", SystemInfoDisplayBoundsToString(result[0].bounds));
  EXPECT_EQ(96, result[0].dpi_x);
  EXPECT_EQ(96, result[0].dpi_y);

  EXPECT_EQ("500,0 200x260", SystemInfoDisplayBoundsToString(result[1].bounds));
  // DPI should be 96 (native dpi) * 200 (display) / 400 (native) when ui scale
  // is 2.
  EXPECT_EQ(96 / 2, result[1].dpi_x);
  EXPECT_EQ(96 / 2, result[1].dpi_y);

  SwapPrimaryDisplay();

  result = GetAllDisplaysInfo();

  ASSERT_EQ(2u, result.size());

  EXPECT_EQ("-500,0 500x600",
            SystemInfoDisplayBoundsToString(result[0].bounds));
  EXPECT_EQ(96, result[0].dpi_x);
  EXPECT_EQ(96, result[0].dpi_y);

  EXPECT_EQ("0,0 200x260", SystemInfoDisplayBoundsToString(result[1].bounds));
  EXPECT_EQ(96 / 2, result[1].dpi_x);
  EXPECT_EQ(96 / 2, result[1].dpi_y);
}

TEST_F(DisplayInfoProviderChromeosTest, GetVisibleArea) {
  UpdateDisplay("640x720*2/o, 400x520/o");
  DisplayUnitInfoList result;
  result = GetAllDisplaysInfo();

  ASSERT_EQ(2u, result.size());

  int64_t display_id;
  ASSERT_TRUE(base::StringToInt64(result[1].id, &display_id))
      << "Display id must be convertable to integer: " << result[1].id;
  ASSERT_TRUE(DisplayExists(display_id)) << display_id << " not found";

  // Default overscan is 5%.
  EXPECT_EQ("304,0 380x494", SystemInfoDisplayBoundsToString(result[1].bounds));
  EXPECT_EQ("13,10,13,10", SystemInfoDisplayInsetsToString(result[1].overscan));

  GetDisplayManager()->SetOverscanInsets(display_id,
                                         gfx::Insets(20, 30, 50, 60));
  result = GetAllDisplaysInfo();

  ASSERT_EQ(2u, result.size());

  EXPECT_EQ(base::Int64ToString(display_id), result[1].id);
  EXPECT_EQ("304,0 310x450", SystemInfoDisplayBoundsToString(result[1].bounds));
  EXPECT_EQ("20,30,50,60", SystemInfoDisplayInsetsToString(result[1].overscan));

  // Set insets for the primary screen. Note that it has 2x scale.
  ASSERT_TRUE(base::StringToInt64(result[0].id, &display_id))
      << "Display id must be convertable to integer: " << result[0].id;
  ASSERT_TRUE(DisplayExists(display_id)) << display_id << " not found";

  EXPECT_EQ("0,0 304x342", SystemInfoDisplayBoundsToString(result[0].bounds));
  EXPECT_EQ("9,8,9,8", SystemInfoDisplayInsetsToString(result[0].overscan));

  GetDisplayManager()->SetOverscanInsets(display_id,
                                         gfx::Insets(10, 20, 30, 40));
  result = GetAllDisplaysInfo();

  ASSERT_EQ(2u, result.size());

  EXPECT_EQ(base::Int64ToString(display_id), result[0].id);
  EXPECT_EQ("0,0 260x320", SystemInfoDisplayBoundsToString(result[0].bounds));
  EXPECT_EQ("10,20,30,40", SystemInfoDisplayInsetsToString(result[0].overscan));
}

TEST_F(DisplayInfoProviderChromeosTest, GetMirroring) {
  UpdateDisplay("600x600, 400x520/o");
  DisplayUnitInfoList result;
  result = GetAllDisplaysInfo();

  ASSERT_EQ(2u, result.size());

  int64_t display_id_primary;
  ASSERT_TRUE(base::StringToInt64(result[0].id, &display_id_primary))
      << "Display id must be convertable to integer: " << result[0].id;
  ASSERT_TRUE(DisplayExists(display_id_primary)) << display_id_primary
                                                 << " not found";

  int64_t display_id_secondary;
  ASSERT_TRUE(base::StringToInt64(result[1].id, &display_id_secondary))
      << "Display id must be convertable to integer: " << result[1].id;
  ASSERT_TRUE(DisplayExists(display_id_secondary)) << display_id_secondary
                                                   << " not found";

  ASSERT_FALSE(GetDisplayManager()->IsInMirrorMode());
  EXPECT_TRUE(result[0].mirroring_source_id.empty());
  EXPECT_TRUE(result[1].mirroring_source_id.empty());

  GetDisplayManager()->SetMirrorMode(display::MirrorMode::kNormal,
                                     base::nullopt);
  ASSERT_TRUE(GetDisplayManager()->IsInMirrorMode());

  result = GetAllDisplaysInfo();

  ASSERT_EQ(1u, result.size());
  EXPECT_EQ(base::Int64ToString(display_id_primary), result[0].id);
  EXPECT_EQ(base::Int64ToString(display_id_primary),
            result[0].mirroring_source_id);

  GetDisplayManager()->SetMirrorMode(display::MirrorMode::kOff, base::nullopt);
  ASSERT_FALSE(GetDisplayManager()->IsInMirrorMode());

  result = GetAllDisplaysInfo();

  ASSERT_EQ(2u, result.size());
  EXPECT_EQ(base::Int64ToString(display_id_primary), result[0].id);
  EXPECT_TRUE(result[0].mirroring_source_id.empty());
  EXPECT_EQ(base::Int64ToString(display_id_secondary), result[1].id);
  EXPECT_TRUE(result[1].mirroring_source_id.empty());
}

TEST_F(DisplayInfoProviderChromeosTest, GetBounds) {
  UpdateDisplay("600x600, 400x520");
  GetDisplayManager()->SetLayoutForCurrentDisplays(
      display::test::CreateDisplayLayout(display_manager(),
                                         display::DisplayPlacement::LEFT, -40));

  DisplayUnitInfoList result = GetAllDisplaysInfo();

  ASSERT_EQ(2u, result.size());
  EXPECT_EQ("0,0 600x600", SystemInfoDisplayBoundsToString(result[0].bounds));
  EXPECT_EQ("-400,-40 400x520",
            SystemInfoDisplayBoundsToString(result[1].bounds));

  GetDisplayManager()->SetLayoutForCurrentDisplays(
      display::test::CreateDisplayLayout(display_manager(),
                                         display::DisplayPlacement::TOP, 40));

  result = GetAllDisplaysInfo();

  ASSERT_EQ(2u, result.size());
  EXPECT_EQ("0,0 600x600", SystemInfoDisplayBoundsToString(result[0].bounds));
  EXPECT_EQ("40,-520 400x520",
            SystemInfoDisplayBoundsToString(result[1].bounds));

  GetDisplayManager()->SetLayoutForCurrentDisplays(
      display::test::CreateDisplayLayout(
          display_manager(), display::DisplayPlacement::BOTTOM, 80));

  result = GetAllDisplaysInfo();
  ASSERT_EQ(2u, result.size());
  EXPECT_EQ("0,0 600x600", SystemInfoDisplayBoundsToString(result[0].bounds));
  EXPECT_EQ("80,600 400x520",
            SystemInfoDisplayBoundsToString(result[1].bounds));
}

TEST_F(DisplayInfoProviderChromeosTest, Layout) {
  UpdateDisplay("500x400,500x400,500x400");

  DisplayUnitInfoList displays = GetAllDisplaysInfo();
  std::string primary_id = displays[0].id;
  ASSERT_EQ(3u, displays.size());

  DisplayLayoutList layout = DisplayInfoProvider::Get()->GetDisplayLayout();

  ASSERT_EQ(2u, layout.size());

  // Confirm layout.
  EXPECT_EQ(displays[1].id, layout[0].id);
  EXPECT_EQ(primary_id, layout[0].parent_id);
  EXPECT_EQ(api::system_display::LAYOUT_POSITION_RIGHT, layout[0].position);
  EXPECT_EQ(0, layout[0].offset);

  EXPECT_EQ(displays[2].id, layout[1].id);
  EXPECT_EQ(layout[0].id, layout[1].parent_id);
  EXPECT_EQ(api::system_display::LAYOUT_POSITION_RIGHT, layout[1].position);
  EXPECT_EQ(0, layout[1].offset);

  // Modify layout.
  layout[0].offset = 100;
  layout[1].parent_id = primary_id;
  layout[1].position = api::system_display::LAYOUT_POSITION_BOTTOM;
  layout[1].offset = -100;

  // Update with modified layout.
  std::string error;
  ASSERT_TRUE(DisplayInfoProvider::Get()->SetDisplayLayout(layout, &error));

  // Get updated layout.
  layout = DisplayInfoProvider::Get()->GetDisplayLayout();

  // Confirm modified layout.
  EXPECT_EQ(displays[1].id, layout[0].id);
  EXPECT_EQ(primary_id, layout[0].parent_id);
  EXPECT_EQ(api::system_display::LAYOUT_POSITION_RIGHT, layout[0].position);
  EXPECT_EQ(100, layout[0].offset);

  EXPECT_EQ(displays[2].id, layout[1].id);
  EXPECT_EQ(primary_id, layout[1].parent_id);
  EXPECT_EQ(api::system_display::LAYOUT_POSITION_BOTTOM, layout[1].position);
  EXPECT_EQ(-100, layout[1].offset);

  // TODO(stevenjb/oshima): Implement and test illegal layout prevention.

  // Test setting invalid layout fails.
  layout[0].parent_id = displays[2].id;
  layout[1].parent_id = displays[1].id;
  EXPECT_FALSE(DisplayInfoProvider::Get()->SetDisplayLayout(layout, &error));
}

TEST_F(DisplayInfoProviderChromeosTest, UnifiedModeLayout) {
  UpdateDisplay("500x300,400x500,300x600,200x300");
  GetDisplayManager()->SetUnifiedDesktopEnabled(true);
  EXPECT_TRUE(GetDisplayManager()->IsInUnifiedMode());

  DisplayUnitInfoList displays = GetAllDisplaysInfo();
  ASSERT_EQ(4u, displays.size());

  // Get the default layout, which should be a horizontal layout.
  DisplayLayoutList default_layout =
      DisplayInfoProvider::Get()->GetDisplayLayout();

  // There is no placement for the primary display.
  ASSERT_EQ(3u, default_layout.size());

  // Confirm the horizontal layout.
  EXPECT_EQ(displays[1].id, default_layout[0].id);
  EXPECT_EQ(displays[0].id, default_layout[0].parent_id);
  EXPECT_EQ(api::system_display::LAYOUT_POSITION_RIGHT,
            default_layout[0].position);
  EXPECT_EQ(0, default_layout[0].offset);
  EXPECT_EQ(displays[2].id, default_layout[1].id);
  EXPECT_EQ(displays[1].id, default_layout[1].parent_id);
  EXPECT_EQ(api::system_display::LAYOUT_POSITION_RIGHT,
            default_layout[1].position);
  EXPECT_EQ(0, default_layout[1].offset);
  EXPECT_EQ(displays[3].id, default_layout[2].id);
  EXPECT_EQ(displays[2].id, default_layout[2].parent_id);
  EXPECT_EQ(api::system_display::LAYOUT_POSITION_RIGHT,
            default_layout[2].position);
  EXPECT_EQ(0, default_layout[2].offset);

  // Create a 2x2 matrix layout.
  // [3][200 x 300] [1][400 x 500]
  // [2][300 x 600] [0][500 x 300]
  DisplayLayoutList layout;
  layout.resize(3u);
  layout[0].id = displays[1].id;
  layout[0].parent_id = displays[3].id;
  layout[0].position = api::system_display::LAYOUT_POSITION_RIGHT;
  layout[1].id = displays[2].id;
  layout[1].parent_id = displays[3].id;
  layout[1].position = api::system_display::LAYOUT_POSITION_BOTTOM;
  layout[2].id = displays[0].id;
  layout[2].parent_id = displays[2].id;
  layout[2].position = api::system_display::LAYOUT_POSITION_RIGHT;

  std::string error;
  ASSERT_TRUE(DisplayInfoProvider::Get()->SetDisplayLayout(layout, &error));
  EXPECT_EQ(gfx::Size(650, 743),
            display::Screen::GetScreen()->GetPrimaryDisplay().size());
  EXPECT_EQ(displays[3].id,
            std::to_string(GetDisplayManager()
                               ->GetPrimaryMirroringDisplayForUnifiedDesktop()
                               ->id()));

  // Confirm the new layout.
  DisplayLayoutList new_layout = DisplayInfoProvider::Get()->GetDisplayLayout();
  ASSERT_EQ(3u, new_layout.size());

  EXPECT_EQ(layout[0].id, new_layout[0].id);
  EXPECT_EQ(layout[0].parent_id, new_layout[0].parent_id);
  EXPECT_EQ(layout[0].position, new_layout[0].position);
  EXPECT_EQ(0, new_layout[0].offset);
  EXPECT_EQ(layout[1].id, new_layout[1].id);
  EXPECT_EQ(layout[1].parent_id, new_layout[1].parent_id);
  EXPECT_EQ(layout[1].position, new_layout[1].position);
  EXPECT_EQ(0, new_layout[1].offset);
  EXPECT_EQ(layout[2].id, new_layout[2].id);
  EXPECT_EQ(layout[2].parent_id, new_layout[2].parent_id);
  EXPECT_EQ(layout[2].position, new_layout[2].position);
  EXPECT_EQ(0, new_layout[2].offset);
}

TEST_F(DisplayInfoProviderChromeosTest, SetBoundsOriginLeftExact) {
  UpdateDisplay("1200x600,520x400");

  const display::Display& secondary = display_manager()->GetSecondaryDisplay();
  api::system_display::DisplayProperties info;
  info.bounds_origin_x.reset(new int(-520));
  info.bounds_origin_y.reset(new int(50));

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_TRUE(success);
  ASSERT_TRUE(error.empty());

  EXPECT_EQ("-520,50 520x400", secondary.bounds().ToString());
}

TEST_F(DisplayInfoProviderChromeosTest, SetBoundsOriginRightExact) {
  UpdateDisplay("1200x600,520x400");

  const display::Display& secondary = display_manager()->GetSecondaryDisplay();
  api::system_display::DisplayProperties info;
  info.bounds_origin_x.reset(new int(1200));
  info.bounds_origin_y.reset(new int(100));

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_TRUE(success);
  ASSERT_TRUE(error.empty());

  EXPECT_EQ("1200,100 520x400", secondary.bounds().ToString());
}

TEST_F(DisplayInfoProviderChromeosTest, SetBoundsOriginTopExact) {
  UpdateDisplay("1200x600,520x400");

  const display::Display& secondary = display_manager()->GetSecondaryDisplay();
  api::system_display::DisplayProperties info;
  info.bounds_origin_x.reset(new int(1100));
  info.bounds_origin_y.reset(new int(-400));

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_TRUE(success);
  ASSERT_TRUE(error.empty());

  EXPECT_EQ("1100,-400 520x400", secondary.bounds().ToString());
}

TEST_F(DisplayInfoProviderChromeosTest, SetBoundsOriginBottomExact) {
  UpdateDisplay("1200x600,520x400");

  const display::Display& secondary = display_manager()->GetSecondaryDisplay();
  api::system_display::DisplayProperties info;
  info.bounds_origin_x.reset(new int(-350));
  info.bounds_origin_y.reset(new int(600));

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_TRUE(success);
  ASSERT_TRUE(error.empty());

  EXPECT_EQ("-350,600 520x400", secondary.bounds().ToString());
}

TEST_F(DisplayInfoProviderChromeosTest, SetBoundsOriginSameCenter) {
  UpdateDisplay("1200x600,520x400");

  const display::Display& secondary = display_manager()->GetSecondaryDisplay();
  api::system_display::DisplayProperties info;
  info.bounds_origin_x.reset(new int(340));
  info.bounds_origin_y.reset(new int(100));

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_TRUE(success);
  ASSERT_TRUE(error.empty());

  EXPECT_EQ("1200,100 520x400", secondary.bounds().ToString());
}

TEST_F(DisplayInfoProviderChromeosTest, SetBoundsOriginLeftOutside) {
  UpdateDisplay("1200x600,520x400");

  const display::Display& secondary = display_manager()->GetSecondaryDisplay();
  api::system_display::DisplayProperties info;
  info.bounds_origin_x.reset(new int(-1040));
  info.bounds_origin_y.reset(new int(100));

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_TRUE(success);
  ASSERT_TRUE(error.empty());

  EXPECT_EQ("-520,100 520x400", secondary.bounds().ToString());
}

TEST_F(DisplayInfoProviderChromeosTest, SetBoundsOriginTopOutside) {
  UpdateDisplay("1200x600,520x400");

  const display::Display& secondary = display_manager()->GetSecondaryDisplay();
  api::system_display::DisplayProperties info;
  info.bounds_origin_x.reset(new int(-360));
  info.bounds_origin_y.reset(new int(-301));

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_TRUE(success);
  ASSERT_TRUE(error.empty());

  EXPECT_EQ("-360,-400 520x400", secondary.bounds().ToString());
}

TEST_F(DisplayInfoProviderChromeosTest,
       SetBoundsOriginLeftButSharesBottomSide) {
  UpdateDisplay("1200x600,1000x100");

  const display::Display& secondary = display_manager()->GetSecondaryDisplay();
  api::system_display::DisplayProperties info;
  info.bounds_origin_x.reset(new int(-650));
  info.bounds_origin_y.reset(new int(700));

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_TRUE(success);
  ASSERT_TRUE(error.empty());

  EXPECT_EQ("-650,600 1000x100", secondary.bounds().ToString());
}

TEST_F(DisplayInfoProviderChromeosTest, SetBoundsOriginRightButSharesTopSide) {
  UpdateDisplay("1200x600,1000x100");

  const display::Display& secondary = display_manager()->GetSecondaryDisplay();
  api::system_display::DisplayProperties info;
  info.bounds_origin_x.reset(new int(850));
  info.bounds_origin_y.reset(new int(-150));

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_TRUE(success);
  ASSERT_TRUE(error.empty());

  EXPECT_EQ("850,-100 1000x100", secondary.bounds().ToString());
}

TEST_F(DisplayInfoProviderChromeosTest, SetBoundsOriginTopButSharesLeftSide) {
  UpdateDisplay("1200x600,1000x100/l");

  const display::Display& secondary = display_manager()->GetSecondaryDisplay();
  api::system_display::DisplayProperties info;
  info.bounds_origin_x.reset(new int(-150));
  info.bounds_origin_y.reset(new int(-650));

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_TRUE(success);
  ASSERT_TRUE(error.empty());

  EXPECT_EQ("-100,-650 100x1000", secondary.bounds().ToString());
}

TEST_F(DisplayInfoProviderChromeosTest,
       SetBoundsOriginBottomButSharesRightSide) {
  UpdateDisplay("1200x600,1000x100/l");

  const display::Display& secondary = display_manager()->GetSecondaryDisplay();
  api::system_display::DisplayProperties info;
  info.bounds_origin_x.reset(new int(1350));
  info.bounds_origin_y.reset(new int(450));

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_TRUE(success);
  ASSERT_TRUE(error.empty());

  EXPECT_EQ("1200,450 100x1000", secondary.bounds().ToString());
}

TEST_F(DisplayInfoProviderChromeosTest, SetBoundsOriginPrimaryHiDPI) {
  UpdateDisplay("1200x600*2,500x500");

  const display::Display& secondary = display_manager()->GetSecondaryDisplay();
  api::system_display::DisplayProperties info;
  info.bounds_origin_x.reset(new int(250));
  info.bounds_origin_y.reset(new int(-100));

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_TRUE(success);
  ASSERT_TRUE(error.empty());

  EXPECT_EQ("600,-100 500x500", secondary.bounds().ToString());
}

TEST_F(DisplayInfoProviderChromeosTest, SetBoundsOriginSecondaryHiDPI) {
  UpdateDisplay("1200x600,600x1000*2");

  const display::Display& secondary = display_manager()->GetSecondaryDisplay();
  api::system_display::DisplayProperties info;
  info.bounds_origin_x.reset(new int(450));
  info.bounds_origin_y.reset(new int(-100));

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_TRUE(success);
  ASSERT_TRUE(error.empty());

  EXPECT_EQ("450,-500 300x500", secondary.bounds().ToString());
}

TEST_F(DisplayInfoProviderChromeosTest, SetBoundsOriginOutOfBounds) {
  UpdateDisplay("1200x600,600x1000*2");

  const display::Display& secondary = display_manager()->GetSecondaryDisplay();
  api::system_display::DisplayProperties info;
  info.bounds_origin_x.reset(new int(0x200001));
  info.bounds_origin_y.reset(new int(-100));

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_FALSE(success);
  ASSERT_EQ("Bounds origin x out of bounds.", error);

  EXPECT_EQ("1200,0 300x500", secondary.bounds().ToString());
}

TEST_F(DisplayInfoProviderChromeosTest, SetBoundsOriginOutOfBoundsNegative) {
  UpdateDisplay("1200x600,600x1000*2");

  const display::Display& secondary = display_manager()->GetSecondaryDisplay();
  api::system_display::DisplayProperties info;
  info.bounds_origin_x.reset(new int(300));
  info.bounds_origin_y.reset(new int(-0x200001));

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_FALSE(success);
  ASSERT_EQ("Bounds origin y out of bounds.", error);

  EXPECT_EQ("1200,0 300x500", secondary.bounds().ToString());
}

TEST_F(DisplayInfoProviderChromeosTest, SetBoundsOriginMaxValues) {
  UpdateDisplay("1200x4600,600x1000*2");

  const display::Display& secondary = display_manager()->GetSecondaryDisplay();
  api::system_display::DisplayProperties info;
  info.bounds_origin_x.reset(new int(200000));
  info.bounds_origin_y.reset(new int(10));

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_TRUE(success);
  EXPECT_TRUE(error.empty());

  EXPECT_EQ("1200,10 300x500", secondary.bounds().ToString());
}

TEST_F(DisplayInfoProviderChromeosTest, SetBoundsOriginOnPrimary) {
  UpdateDisplay("1200x600,600x1000*2");

  const display::Display& secondary = display_manager()->GetSecondaryDisplay();
  api::system_display::DisplayProperties info;
  info.bounds_origin_x.reset(new int(300));
  info.is_primary.reset(new bool(true));

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_FALSE(success);
  ASSERT_EQ("Bounds origin not allowed for the primary display.", error);

  EXPECT_EQ("1200,0 300x500", secondary.bounds().ToString());
  // The operation failed because the primary property would be set before
  // setting bounds. The primary display shouldn't have been changed, though.
  EXPECT_NE(display::Screen::GetScreen()->GetPrimaryDisplay().id(),
            secondary.id());
}

TEST_F(DisplayInfoProviderChromeosTest, SetBoundsOriginWithMirroring) {
  UpdateDisplay("1200x600,600x1000*2");

  const display::Display& secondary = display_manager()->GetSecondaryDisplay();
  const display::Display& primary =
      display::Screen::GetScreen()->GetPrimaryDisplay();

  api::system_display::DisplayProperties info;
  info.bounds_origin_x.reset(new int(300));
  info.mirroring_source_id.reset(
      new std::string(base::Int64ToString(primary.id())));

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_FALSE(success);
  ASSERT_EQ("No other parameter should be set alongside mirroringSourceId.",
            error);
}

TEST_F(DisplayInfoProviderChromeosTest, SetRotation) {
  UpdateDisplay("1200x600,600x1000*2");

  const display::Display& secondary = display_manager()->GetSecondaryDisplay();
  api::system_display::DisplayProperties info;
  info.rotation.reset(new int(90));

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_TRUE(success);
  EXPECT_TRUE(error.empty());

  EXPECT_EQ("1200,0 500x300", secondary.bounds().ToString());
  EXPECT_EQ(display::Display::ROTATE_90, secondary.rotation());

  info.rotation.reset(new int(270));
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_TRUE(success);
  EXPECT_TRUE(error.empty());

  EXPECT_EQ("1200,0 500x300", secondary.bounds().ToString());
  EXPECT_EQ(display::Display::ROTATE_270, secondary.rotation());

  info.rotation.reset(new int(180));
  // Switch primary display.
  info.is_primary.reset(new bool(true));
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_TRUE(success);
  EXPECT_TRUE(error.empty());

  EXPECT_EQ("0,0 300x500", secondary.bounds().ToString());
  EXPECT_EQ(display::Display::ROTATE_180, secondary.rotation());
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().id(),
            secondary.id());

  info.rotation.reset(new int(0));
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_TRUE(success);
  EXPECT_TRUE(error.empty());

  EXPECT_EQ("0,0 300x500", secondary.bounds().ToString());
  EXPECT_EQ(display::Display::ROTATE_0, secondary.rotation());
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().id(),
            secondary.id());
}

// Tests that rotation changes made before entering tablet mode are restored
// upon exiting tablet mode, and that a rotation lock is not set.
TEST_F(DisplayInfoProviderChromeosTest, SetRotationBeforeTabletMode) {
  ash::ScreenOrientationController* screen_orientation_controller =
      ash::Shell::Get()->screen_orientation_controller();
  api::system_display::DisplayProperties info;
  info.rotation.reset(new int(90));

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(
      base::Int64ToString(display::Display::InternalDisplayId()), info,
      &success, &error);

  ASSERT_TRUE(success);
  EXPECT_TRUE(error.empty());
  EXPECT_FALSE(screen_orientation_controller->rotation_locked());

  // Entering tablet mode enables accelerometer screen rotations.
  EnableTabletMode(true);
  // Rotation lock should not activate because DisplayInfoProvider::SetInfo()
  // was called when not in tablet mode.
  EXPECT_FALSE(screen_orientation_controller->rotation_locked());

  // ScreenOrientationController rotations override display info.
  ash::ScreenOrientationControllerTestApi test_api(
      screen_orientation_controller);
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());

  // Exiting tablet mode should restore the initial rotation
  EnableTabletMode(false);
  EXPECT_EQ(display::Display::ROTATE_90, GetCurrentInternalDisplayRotation());
}

// Tests that rotation changes made during tablet mode lock the display
// against accelerometer rotations, and is set as user rotation locked.
TEST_F(DisplayInfoProviderChromeosTest, SetRotationDuringTabletMode) {
  // Entering tablet mode enables accelerometer screen rotations.
  EnableTabletMode(true);

  ASSERT_FALSE(
      ash::Shell::Get()->screen_orientation_controller()->rotation_locked());
  ASSERT_FALSE(ash::Shell::Get()
                   ->screen_orientation_controller()
                   ->user_rotation_locked());

  api::system_display::DisplayProperties info;
  info.rotation.reset(new int(90));

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(
      base::Int64ToString(display::Display::InternalDisplayId()), info,
      &success, &error);

  ASSERT_TRUE(success);
  EXPECT_TRUE(error.empty());
  EXPECT_TRUE(
      ash::Shell::Get()->screen_orientation_controller()->rotation_locked());
  EXPECT_TRUE(ash::Shell::Get()
                  ->screen_orientation_controller()
                  ->user_rotation_locked());
}

TEST_F(DisplayInfoProviderChromeosTest, SetInvalidRotation) {
  UpdateDisplay("1200x600,600x1000*2");

  const display::Display& secondary = display_manager()->GetSecondaryDisplay();
  api::system_display::DisplayProperties info;
  info.rotation.reset(new int(91));

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_FALSE(success);
  EXPECT_EQ("Invalid rotation.", error);
}

TEST_F(DisplayInfoProviderChromeosTest, SetNegativeOverscan) {
  UpdateDisplay("1200x600,600x1000*2");

  const display::Display& secondary = display_manager()->GetSecondaryDisplay();
  api::system_display::DisplayProperties info;
  info.overscan.reset(new api::system_display::Insets);
  info.overscan->left = -10;

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_FALSE(success);
  EXPECT_EQ("Negative overscan not allowed.", error);

  EXPECT_EQ("1200,0 300x500", secondary.bounds().ToString());

  info.overscan->left = 0;
  info.overscan->right = -200;

  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_FALSE(success);
  EXPECT_EQ("Negative overscan not allowed.", error);

  EXPECT_EQ("1200,0 300x500", secondary.bounds().ToString());

  info.overscan->right = 0;
  info.overscan->top = -300;

  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_FALSE(success);
  EXPECT_EQ("Negative overscan not allowed.", error);

  EXPECT_EQ("1200,0 300x500", secondary.bounds().ToString());

  info.overscan->right = 0;
  info.overscan->top = -1000;

  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_FALSE(success);
  EXPECT_EQ("Negative overscan not allowed.", error);

  EXPECT_EQ("1200,0 300x500", secondary.bounds().ToString());

  info.overscan->right = 0;
  info.overscan->top = 0;

  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_TRUE(success);
  EXPECT_TRUE(error.empty());

  EXPECT_EQ("1200,0 300x500", secondary.bounds().ToString());
}

TEST_F(DisplayInfoProviderChromeosTest, SetOverscanLargerThanHorizontalBounds) {
  UpdateDisplay("1200x600,600x1000*2");

  const display::Display& secondary = display_manager()->GetSecondaryDisplay();
  api::system_display::DisplayProperties info;
  info.overscan.reset(new api::system_display::Insets);
  // Horizontal overscan is 151, which would make the bounds width 149.
  info.overscan->left = 50;
  info.overscan->top = 10;
  info.overscan->right = 101;
  info.overscan->bottom = 20;

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_FALSE(success);
  EXPECT_EQ("Horizontal overscan is more than half of the screen width.",
            error);
}

TEST_F(DisplayInfoProviderChromeosTest, SetOverscanLargerThanVerticalBounds) {
  UpdateDisplay("1200x600,600x1000");

  const display::Display& secondary = display_manager()->GetSecondaryDisplay();
  api::system_display::DisplayProperties info;
  info.overscan.reset(new api::system_display::Insets);
  // Vertical overscan is 501, which would make the bounds height 499.
  info.overscan->left = 20;
  info.overscan->top = 250;
  info.overscan->right = 101;
  info.overscan->bottom = 251;

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_FALSE(success);
  EXPECT_EQ("Vertical overscan is more than half of the screen height.", error);
}

TEST_F(DisplayInfoProviderChromeosTest, SetOverscan) {
  UpdateDisplay("1200x600,600x1000*2");

  const display::Display& secondary = display_manager()->GetSecondaryDisplay();
  api::system_display::DisplayProperties info;
  info.overscan.reset(new api::system_display::Insets);
  info.overscan->left = 20;
  info.overscan->top = 199;
  info.overscan->right = 130;
  info.overscan->bottom = 51;

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_TRUE(success);
  EXPECT_TRUE(error.empty());

  EXPECT_EQ("1200,0 150x250", secondary.bounds().ToString());
  const gfx::Insets overscan =
      GetDisplayManager()->GetOverscanInsets(secondary.id());

  EXPECT_EQ(20, overscan.left());
  EXPECT_EQ(199, overscan.top());
  EXPECT_EQ(130, overscan.right());
  EXPECT_EQ(51, overscan.bottom());
}

TEST_F(DisplayInfoProviderChromeosTest, SetOverscanForInternal) {
  UpdateDisplay("1200x600,600x1000*2");
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
          .SetFirstDisplayAsInternalDisplay();

  api::system_display::DisplayProperties info;
  info.overscan.reset(new api::system_display::Insets);
  // Vertical overscan is 501, which would make the bounds height 499.
  info.overscan->left = 20;
  info.overscan->top = 20;
  info.overscan->right = 20;
  info.overscan->bottom = 20;

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(
      base::Int64ToString(internal_display_id), info, &success, &error);

  ASSERT_FALSE(success);
  EXPECT_EQ("Overscan changes not allowed for the internal monitor.", error);
}

TEST_F(DisplayInfoProviderChromeosTest, DisplayMode) {
  UpdateDisplay("1200x600,600x1000");

  DisplayUnitInfoList result = GetAllDisplaysInfo();
  ASSERT_GE(result.size(), 1u);
  const api::system_display::DisplayUnitInfo& primary_info = result[0];
  // Ensure that we have two modes for the primary display so that we can
  // test chaning modes.
  ASSERT_GE(primary_info.modes.size(), 2u);

  // Get the currently active mode and one other mode to switch to.
  int64_t id;
  base::StringToInt64(primary_info.id, &id);
  display::ManagedDisplayMode active_mode;
  EXPECT_TRUE(GetDisplayManager()->GetActiveModeForDisplayId(id, &active_mode));
  const api::system_display::DisplayMode* cur_mode = nullptr;
  const api::system_display::DisplayMode* other_mode = nullptr;
  for (const auto& mode : primary_info.modes) {
    if (mode.is_selected)
      cur_mode = &mode;
    else if (!other_mode)
      other_mode = &mode;
    if (cur_mode && other_mode)
      break;
  }
  ASSERT_TRUE(cur_mode);
  ASSERT_TRUE(other_mode);
  ASSERT_NE(other_mode, cur_mode);

  // Verify that other_mode differs from the active mode.
  display::ManagedDisplayMode other_mode_ash(
      gfx::Size(other_mode->width_in_native_pixels,
                other_mode->height_in_native_pixels),
      active_mode.refresh_rate(), active_mode.is_interlaced(),
      active_mode.native(), other_mode->ui_scale,
      other_mode->device_scale_factor);
  EXPECT_FALSE(active_mode.IsEquivalent(other_mode_ash));

  // Switch modes.
  api::system_display::DisplayProperties info;
  info.display_mode =
      api::system_display::DisplayMode::FromValue(*other_mode->ToValue());

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(base::Int64ToString(id), info, &success, &error);
  ASSERT_TRUE(success);

  // Verify that other_mode now matches the active mode.
  EXPECT_TRUE(GetDisplayManager()->GetActiveModeForDisplayId(id, &active_mode));
  EXPECT_TRUE(active_mode.IsEquivalent(other_mode_ash));
}

TEST_F(DisplayInfoProviderChromeosTest, CustomTouchCalibrationInternal) {
  UpdateDisplay("1200x600,600x1000*2");
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
          .SetFirstDisplayAsInternalDisplay();

  InitExternalTouchDevices(internal_display_id);

  std::string id = base::Int64ToString(internal_display_id);

  std::string error;
  std::string expected_err =
      "Display Id(" + id + ") is an internal display." +
      " Internal displays cannot be calibrated for touch.";
  bool success = DisplayInfoProvider::Get()->StartCustomTouchCalibration(
      id, &error);

  ASSERT_FALSE(success);
  EXPECT_EQ(expected_err, error);
}

TEST_F(DisplayInfoProviderChromeosTest, CustomTouchCalibrationWithoutStart) {
  UpdateDisplay("1200x600,600x1000*2");

  api::system_display::TouchCalibrationPairQuad pairs;
  api::system_display::Bounds bounds;

  std::string error;
  std::string expected_err =
      DisplayInfoProviderChromeOS::kCompleteCalibrationCalledBeforeStartError;
  bool success = DisplayInfoProvider::Get()->CompleteCustomTouchCalibration(
      pairs, bounds, &error);

  ASSERT_FALSE(success);
  EXPECT_EQ(expected_err, error);
}


TEST_F(DisplayInfoProviderChromeosTest, CustomTouchCalibrationNonTouchDisplay) {
  UpdateDisplay("1200x600,600x1000*2");

  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
          .SetFirstDisplayAsInternalDisplay();

  display::DisplayIdList display_id_list =
      display_manager()->GetCurrentDisplayIdList();

  // Pick the non internal display Id.
  const int64_t display_id = display_id_list[0] == internal_display_id
                                 ? display_id_list[1]
                                 : display_id_list[0];

  ui::InputDeviceClientTestApi().SetTouchscreenDevices({});
  std::string id = base::Int64ToString(display_id);

  std::string error;
  std::string expected_err =
      DisplayInfoProviderChromeOS::kNoExternalTouchDevicePresent;
  bool success = DisplayInfoProvider::Get()->StartCustomTouchCalibration(
      id, &error);

  // Since no external touch devices are present, the calibration would fail.
  EXPECT_FALSE(success);
  EXPECT_EQ(error, expected_err);

  InitExternalTouchDevices(display_id);
  error.clear();

  success = DisplayInfoProvider::Get()->StartCustomTouchCalibration(id, &error);
  // If an external touch device is present, the calibration should proceed.
  EXPECT_TRUE(success);
}

TEST_F(DisplayInfoProviderChromeosTest, CustomTouchCalibrationNegativeBounds) {
  UpdateDisplay("1200x600,600x1000*2");

  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();

  display::DisplayIdList display_id_list =
      display_manager()->GetCurrentDisplayIdList();

  // Pick the non internal display Id.
  const int64_t display_id = display_id_list[0] == internal_display_id
                                 ? display_id_list[1]
                                 : display_id_list[0];

  InitExternalTouchDevices(display_id);

  std::string id = base::Int64ToString(display_id);

  api::system_display::TouchCalibrationPairQuad pairs;
  api::system_display::Bounds bounds;
  bounds.width = -1;

  std::string error;
  DisplayInfoProvider::Get()->StartCustomTouchCalibration(id, &error);
  error.clear();
  bool success = DisplayInfoProvider::Get()->CompleteCustomTouchCalibration(
      pairs, bounds, &error);

  std::string expected_err =
      DisplayInfoProviderChromeOS::kTouchBoundsNegativeError;

  ASSERT_FALSE(success);
  EXPECT_EQ(expected_err, error);

  error.clear();
  bounds.width = 0;
  bounds.height = -1;

  DisplayInfoProvider::Get()->StartCustomTouchCalibration(id, &error);
  error.clear();
  success = DisplayInfoProvider::Get()->CompleteCustomTouchCalibration(
      pairs, bounds, &error);
  ASSERT_FALSE(success);
  EXPECT_EQ(expected_err, error);
}

TEST_F(DisplayInfoProviderChromeosTest, CustomTouchCalibrationInvalidPoints) {
  UpdateDisplay("1200x600,600x1000*2");

  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();

  display::DisplayIdList display_id_list =
      display_manager()->GetCurrentDisplayIdList();

  // Pick the non internal display Id.
  const int64_t display_id = display_id_list[0] == internal_display_id
                                 ? display_id_list[1]
                                 : display_id_list[0];

  InitExternalTouchDevices(display_id);

  std::string id = base::Int64ToString(display_id);

  api::system_display::TouchCalibrationPairQuad pairs;
  api::system_display::Bounds bounds;

  pairs.pair1.display_point.x = -1;
  std::string error;
  DisplayInfoProvider::Get()->StartCustomTouchCalibration(id, &error);
  error.clear();
  bool success = DisplayInfoProvider::Get()->CompleteCustomTouchCalibration(
      pairs, bounds, &error);

  std::string expected_err =
      DisplayInfoProviderChromeOS::kTouchCalibrationPointsNegativeError;

  ASSERT_FALSE(success);
  EXPECT_EQ(expected_err, error);

  error.clear();
  bounds.width = 1;
  pairs.pair1.display_point.x = 2;
  expected_err =
      DisplayInfoProviderChromeOS::kTouchCalibrationPointsTooLargeError;

  DisplayInfoProvider::Get()->StartCustomTouchCalibration(id, &error);
  error.clear();
  success = DisplayInfoProvider::Get()->CompleteCustomTouchCalibration(
      pairs, bounds, &error);
  ASSERT_FALSE(success);
  EXPECT_EQ(expected_err, error);
}

TEST_F(DisplayInfoProviderChromeosTest, CustomTouchCalibrationSuccess) {
  UpdateDisplay("1200x600,600x1000*2");

  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();

  display::DisplayIdList display_id_list =
      display_manager()->GetCurrentDisplayIdList();

  // Pick the non internal display Id.
  const int64_t display_id = display_id_list[0] == internal_display_id
                                 ? display_id_list[1]
                                 : display_id_list[0];

  InitExternalTouchDevices(display_id);

  std::string error;
  bool success = DisplayInfoProvider::Get()->StartCustomTouchCalibration(
      base::Int64ToString(display_id), &error);

  EXPECT_TRUE(success);
}

TEST_F(DisplayInfoProviderChromeosTest, GetDisplayZoomFactor) {
  UpdateDisplay("1200x600,1600x1000*2");
  display::DisplayIdList display_id_list =
      display_manager()->GetCurrentDisplayIdList();

  float zoom_factor_1 = 1.23f;
  float zoom_factor_2 = 2.34f;
  display_manager()->UpdateZoomFactor(display_id_list[0], zoom_factor_1);
  display_manager()->UpdateZoomFactor(display_id_list[1], zoom_factor_2);

  DisplayUnitInfoList displays = GetAllDisplaysInfo();

  for (const auto& display : displays) {
    if (display.id == base::Int64ToString(display_id_list[0]))
      EXPECT_EQ(display.display_zoom_factor, zoom_factor_1);
    if (display.id == base::Int64ToString(display_id_list[1]))
      EXPECT_EQ(display.display_zoom_factor, zoom_factor_2);
  }
}

TEST_F(DisplayInfoProviderChromeosTest, SetDisplayZoomFactor) {
  UpdateDisplay("1200x600, 1600x1000#1600x1000");
  display::DisplayIdList display_id_list =
      display_manager()->GetCurrentDisplayIdList();

  float zoom_factor_1 = 1.23f;
  float zoom_factor_2 = 2.34f;
  display_manager()->UpdateZoomFactor(display_id_list[0], zoom_factor_2);
  display_manager()->UpdateZoomFactor(display_id_list[1], zoom_factor_1);

  EXPECT_EQ(display_manager()->GetZoomFactorForDisplay(display_id_list[0]),
            zoom_factor_2);
  EXPECT_EQ(display_manager()->GetZoomFactorForDisplay(display_id_list[1]),
            zoom_factor_1);

  // After update, display 1 should have |final_zoom_factor_1| as its zoom
  // factor and display 2 should have |final_zoom_factor_2| as its zoom factor.
  float final_zoom_factor_1 = zoom_factor_1;
  float final_zoom_factor_2 = zoom_factor_2;

  api::system_display::DisplayProperties info;
  info.display_zoom_factor = std::make_unique<double>(zoom_factor_1);

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(base::Int64ToString(display_id_list[0]), info,
                         &success, &error);
  ASSERT_TRUE(success);

  EXPECT_EQ(display_manager()->GetZoomFactorForDisplay(display_id_list[0]),
            final_zoom_factor_1);
  // Display 2 has not been updated yet, so it will still have the old zoom
  // factor.
  EXPECT_EQ(display_manager()->GetZoomFactorForDisplay(display_id_list[1]),
            zoom_factor_1);

  info.display_zoom_factor = std::make_unique<double>(zoom_factor_2);
  CallSetDisplayUnitInfo(base::Int64ToString(display_id_list[1]), info,
                         &success, &error);
  ASSERT_TRUE(success);

  // Both displays should now have the correct zoom factor set.
  EXPECT_EQ(display_manager()->GetZoomFactorForDisplay(display_id_list[0]),
            final_zoom_factor_1);
  EXPECT_EQ(display_manager()->GetZoomFactorForDisplay(display_id_list[1]),
            final_zoom_factor_2);

  std::string expected_err =
      "Zoom value is out of range for display with id: " +
      base::Int64ToString(display_id_list[0]);

  // This zoom factor when applied to the display with width 1200, will result
  // in an effective width greater than 4096, which is out of range.
  float invalid_zoom_factor_1 = 0.285f;
  info.display_zoom_factor = std::make_unique<double>(invalid_zoom_factor_1);
  CallSetDisplayUnitInfo(base::Int64ToString(display_id_list[0]), info,
                         &success, &error);
  EXPECT_FALSE(success);
  EXPECT_EQ(error, expected_err);

  success = false;
  error = "";

  // This zoom factor when applied to the display with width 1200, will result
  // in an effective width greater less than 640, which is out of range.
  float invalid_zoom_factor_2 = 1.88f;
  info.display_zoom_factor = std::make_unique<double>(invalid_zoom_factor_2);
  CallSetDisplayUnitInfo(base::Int64ToString(display_id_list[0]), info,
                         &success, &error);
  EXPECT_FALSE(success);
  EXPECT_EQ(error, expected_err);

  success = false;
  error = "";

  // Initalize displays that have bounds outside the valid width range of 640px
  // to 4096px.
  UpdateDisplay("400x400, 4500x1000#4500x1000");

  display_id_list = display_manager()->GetCurrentDisplayIdList();

  // Results in a logical width of 500px. This is below the 640px threshold but
  // is valid because the initial width was 400px, so the logical width now
  // allows a minimum width of 400px.
  float valid_zoom_factor_1 = 0.8f;
  info.display_zoom_factor = std::make_unique<double>(valid_zoom_factor_1);
  CallSetDisplayUnitInfo(base::Int64ToString(display_id_list[0]), info,
                         &success, &error);
  EXPECT_TRUE(success);

  // Results in a logical width of 4200px. This is above the 4096px threshold
  // but is valid because the initial width was 4500px, so logical width of up
  // to 4500px is allowed in this case.
  float valid_zoom_factor_2 = 1.07f;
  info.display_zoom_factor = std::make_unique<double>(valid_zoom_factor_2);
  CallSetDisplayUnitInfo(base::Int64ToString(display_id_list[1]), info,
                         &success, &error);
  EXPECT_TRUE(success);

  float valid_zoom_factor_3 = 0.5f;
  info.display_zoom_factor = std::make_unique<double>(valid_zoom_factor_3);
  CallSetDisplayUnitInfo(base::Int64ToString(display_id_list[0]), info,
                         &success, &error);
  EXPECT_TRUE(success);

  float valid_zoom_factor_4 = 2.f;
  info.display_zoom_factor = std::make_unique<double>(valid_zoom_factor_4);
  CallSetDisplayUnitInfo(base::Int64ToString(display_id_list[1]), info,
                         &success, &error);
  EXPECT_TRUE(success);
}

class DisplayInfoProviderChromeosTouchviewTest
    : public DisplayInfoProviderChromeosTest {
 public:
  DisplayInfoProviderChromeosTouchviewTest() {}

  ~DisplayInfoProviderChromeosTouchviewTest() override {}

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFirstDisplayAsInternal);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kAshEnableTabletMode);
    DisplayInfoProviderChromeosTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayInfoProviderChromeosTouchviewTest);
};

TEST_F(DisplayInfoProviderChromeosTouchviewTest, GetTabletMode) {
  UpdateDisplay("500x600,400x520");

  // Check initial state.
  DisplayUnitInfoList result = GetAllDisplaysInfo();
  ASSERT_EQ(2u, result.size());
  EXPECT_TRUE(result[0].has_accelerometer_support);
  EXPECT_FALSE(result[0].is_tablet_mode);
  EXPECT_FALSE(result[1].has_accelerometer_support);
  EXPECT_FALSE(result[1].is_tablet_mode);

  EnableTabletMode(true);
  result = GetAllDisplaysInfo();
  ASSERT_EQ(2u, result.size());
  EXPECT_TRUE(result[0].has_accelerometer_support);
  ASSERT_TRUE(result[0].is_tablet_mode);
  EXPECT_TRUE(*result[0].is_tablet_mode);
  EXPECT_FALSE(result[1].has_accelerometer_support);
  EXPECT_FALSE(result[1].is_tablet_mode);
}

TEST_F(DisplayInfoProviderChromeosTest, SetMIXEDMode) {
  {
    // Mirroring source id not specified error.
    api::system_display::MirrorModeInfo info;
    info.mode = api::system_display::MIRROR_MODE_MIXED;
    std::string error;
    EXPECT_FALSE(DisplayInfoProvider::Get()->SetMirrorMode(info, &error));
    EXPECT_EQ(DisplayInfoProviderChromeOS::kMirrorModeSourceIdNotSpecifiedError,
              error);
  }

  {
    // Mirroring destination ids not specified error.
    api::system_display::MirrorModeInfo info;
    info.mode = api::system_display::MIRROR_MODE_MIXED;
    info.mirroring_source_id.reset(new std::string("1000000"));
    std::string error;
    EXPECT_FALSE(DisplayInfoProvider::Get()->SetMirrorMode(info, &error));
    EXPECT_EQ(
        DisplayInfoProviderChromeOS::kMirrorModeDestinationIdsNotSpecifiedError,
        error);
  }

  {
    // Mirroring source id in bad format error.
    api::system_display::MirrorModeInfo info;
    info.mode = api::system_display::MIRROR_MODE_MIXED;
    info.mirroring_source_id.reset(new std::string("bad_format_id"));
    info.mirroring_destination_ids.reset(new std::vector<std::string>());
    std::string error;
    EXPECT_FALSE(DisplayInfoProvider::Get()->SetMirrorMode(info, &error));
    EXPECT_EQ(DisplayInfoProviderChromeOS::kMirrorModeSourceIdBadFormatError,
              error);
  }

  {
    // Mirroring destination id in bad format error.
    api::system_display::MirrorModeInfo info;
    info.mode = api::system_display::MIRROR_MODE_MIXED;
    info.mirroring_source_id.reset(new std::string("1000000"));
    info.mirroring_destination_ids.reset(new std::vector<std::string>());
    info.mirroring_destination_ids->emplace_back("bad_format_id");
    std::string error;
    EXPECT_FALSE(DisplayInfoProvider::Get()->SetMirrorMode(info, &error));
    EXPECT_EQ(
        DisplayInfoProviderChromeOS::kMirrorModeDestinationIdBadFormatError,
        error);
  }

  {
    // Single display error.
    EXPECT_EQ(1U, display_manager()->num_connected_displays());
    api::system_display::MirrorModeInfo info;
    info.mode = api::system_display::MIRROR_MODE_MIXED;
    info.mirroring_source_id.reset(new std::string("1000000"));
    info.mirroring_destination_ids.reset(new std::vector<std::string>());
    std::string error;
    EXPECT_FALSE(DisplayInfoProvider::Get()->SetMirrorMode(info, &error));
    EXPECT_EQ(DisplayInfoProviderChromeOS::kMirrorModeSingleDisplayError,
              error);
  }

  // Add more displays.
  UpdateDisplay("200x200,600x600,700x700");
  display::DisplayIdList id_list = display_manager()->GetCurrentDisplayIdList();
  EXPECT_EQ(3U, id_list.size());

  {
    // Mirroring source id not found error.
    api::system_display::MirrorModeInfo info;
    info.mode = api::system_display::MIRROR_MODE_MIXED;
    info.mirroring_source_id.reset(new std::string("1000000"));
    info.mirroring_destination_ids.reset(new std::vector<std::string>());
    std::string error;
    EXPECT_FALSE(DisplayInfoProvider::Get()->SetMirrorMode(info, &error));
    EXPECT_EQ(DisplayInfoProviderChromeOS::kMirrorModeSourceIdNotFoundError,
              error);
  }

  {
    // Mirroring destination ids empty error.
    api::system_display::MirrorModeInfo info;
    info.mode = api::system_display::MIRROR_MODE_MIXED;
    info.mirroring_source_id.reset(
        new std::string(base::Int64ToString(id_list[0])));
    info.mirroring_destination_ids.reset(new std::vector<std::string>());
    std::string error;
    EXPECT_FALSE(DisplayInfoProvider::Get()->SetMirrorMode(info, &error));
    EXPECT_EQ(DisplayInfoProviderChromeOS::kMirrorModeDestinationIdsEmptyError,
              error);
  }

  {
    // Mirroring destination ids not found error.
    api::system_display::MirrorModeInfo info;
    info.mode = api::system_display::MIRROR_MODE_MIXED;
    info.mirroring_source_id.reset(
        new std::string(base::Int64ToString(id_list[0])));
    info.mirroring_destination_ids.reset(new std::vector<std::string>());
    info.mirroring_destination_ids->emplace_back(
        base::Int64ToString(display::kInvalidDisplayId));
    std::string error;
    EXPECT_FALSE(DisplayInfoProvider::Get()->SetMirrorMode(info, &error));
    EXPECT_EQ(
        DisplayInfoProviderChromeOS::kMirrorModeDestinationIdNotFoundError,
        error);
  }

  {
    // Duplicate display id error.
    api::system_display::MirrorModeInfo info;
    info.mode = api::system_display::MIRROR_MODE_MIXED;
    info.mirroring_source_id.reset(
        new std::string(base::Int64ToString(id_list[0])));
    info.mirroring_destination_ids.reset(new std::vector<std::string>());
    info.mirroring_destination_ids->emplace_back(
        base::Int64ToString(id_list[0]));
    std::string error;
    EXPECT_FALSE(DisplayInfoProvider::Get()->SetMirrorMode(info, &error));
    EXPECT_EQ(DisplayInfoProviderChromeOS::kMirrorModeDuplicateIdError, error);
  }

  {
    // Turn on mixed mirror mode (mirroring from the first display to the second
    // one).
    api::system_display::MirrorModeInfo info;
    info.mode = api::system_display::MIRROR_MODE_MIXED;
    info.mirroring_source_id.reset(
        new std::string(base::Int64ToString(id_list[0])));
    info.mirroring_destination_ids.reset(new std::vector<std::string>());
    info.mirroring_destination_ids->emplace_back(
        base::Int64ToString(id_list[1]));
    std::string error;
    EXPECT_TRUE(DisplayInfoProvider::Get()->SetMirrorMode(info, &error));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
    EXPECT_EQ(id_list[0], display_manager()->mirroring_source_id());
    const display::Displays software_mirroring_display_list =
        display_manager()->software_mirroring_display_list();
    EXPECT_EQ(1U, software_mirroring_display_list.size());
    EXPECT_EQ(id_list[1], software_mirroring_display_list[0].id());

    // Turn off mixed mirror mode.
    info.mode = api::system_display::MIRROR_MODE_OFF;
    EXPECT_TRUE(DisplayInfoProvider::Get()->SetMirrorMode(info, &error));
    EXPECT_TRUE(error.empty());
    EXPECT_FALSE(display_manager()->IsInMirrorMode());
  }
}

}  // namespace
}  // namespace extensions
