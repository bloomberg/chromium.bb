// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_display/display_info_provider.h"

#include <stdint.h>

#include "ash/ash_switches.h"
#include "ash/display/display_manager.h"
#include "ash/display/screen_orientation_controller_chromeos.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/display_manager_test_api.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "extensions/common/api/system_display.h"
#include "ui/gfx/display.h"
#include "ui/gfx/geometry/rect.h"

namespace extensions {
namespace {

class DisplayInfoProviderChromeosTest : public ash::test::AshTestBase {
 public:
  DisplayInfoProviderChromeosTest() {}

  ~DisplayInfoProviderChromeosTest() override {}

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kAshUseFirstDisplayAsInternal);
    ash::test::AshTestBase::SetUp();
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
    const gfx::Display& display =
        GetDisplayManager()->GetDisplayForId(display_id);
    return display.id() != gfx::Display::kInvalidDisplayID;
  }

  ash::DisplayManager* GetDisplayManager() const {
    return ash::Shell::GetInstance()->display_manager();
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

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayInfoProviderChromeosTest);
};

TEST_F(DisplayInfoProviderChromeosTest, GetBasic) {
  UpdateDisplay("500x600,400x520");
  DisplayInfo result = DisplayInfoProvider::Get()->GetAllDisplaysInfo();

  ASSERT_EQ(2u, result.size());

  int64_t display_id;
  ASSERT_TRUE(base::StringToInt64(result[0]->id, &display_id))
      << "Display id must be convertable to integer: " << result[0]->id;

  ASSERT_TRUE(DisplayExists(display_id)) << display_id << " not found";
  EXPECT_EQ("0,0 500x600", SystemInfoDisplayBoundsToString(result[0]->bounds));
  EXPECT_EQ("0,0,0,0", SystemInfoDisplayInsetsToString(result[0]->overscan));
  EXPECT_EQ(0, result[0]->rotation);
  EXPECT_TRUE(result[0]->is_primary);
  EXPECT_EQ(96, result[0]->dpi_x);
  EXPECT_EQ(96, result[0]->dpi_y);
  EXPECT_TRUE(result[0]->mirroring_source_id.empty());
  EXPECT_TRUE(result[0]->is_enabled);

  ASSERT_TRUE(base::StringToInt64(result[1]->id, &display_id))
      << "Display id must be convertable to integer: " << result[0]->id;

  ASSERT_TRUE(DisplayExists(display_id)) << display_id << " not found";
  EXPECT_EQ(GetDisplayManager()->GetDisplayNameForId(display_id),
            result[1]->name);
  // The second display is positioned left of the primary display, whose width
  // is 500.
  EXPECT_EQ("500,0 400x520",
            SystemInfoDisplayBoundsToString(result[1]->bounds));
  EXPECT_EQ("0,0,0,0", SystemInfoDisplayInsetsToString(result[1]->overscan));
  EXPECT_EQ(0, result[1]->rotation);
  EXPECT_FALSE(result[1]->is_primary);
  EXPECT_EQ(96, result[1]->dpi_x);
  EXPECT_EQ(96, result[1]->dpi_y);
  EXPECT_TRUE(result[1]->mirroring_source_id.empty());
  EXPECT_TRUE(result[1]->is_enabled);
}

TEST_F(DisplayInfoProviderChromeosTest, GetWithUnifiedDesktop) {
  UpdateDisplay("500x600,400x520");

  // Check initial state.
  EXPECT_FALSE(GetDisplayManager()->IsInUnifiedMode());
  DisplayInfo result = DisplayInfoProvider::Get()->GetAllDisplaysInfo();

  ASSERT_EQ(2u, result.size());

  int64_t display_id;
  ASSERT_TRUE(base::StringToInt64(result[0]->id, &display_id))
      << "Display id must be convertable to integer: " << result[0]->id;

  ASSERT_TRUE(DisplayExists(display_id)) << display_id << " not found";
  EXPECT_EQ("0,0 500x600", SystemInfoDisplayBoundsToString(result[0]->bounds));
  EXPECT_EQ("0,0,0,0", SystemInfoDisplayInsetsToString(result[0]->overscan));
  EXPECT_EQ(0, result[0]->rotation);
  EXPECT_TRUE(result[0]->is_primary);
  EXPECT_EQ(96, result[0]->dpi_x);
  EXPECT_EQ(96, result[0]->dpi_y);
  EXPECT_TRUE(result[0]->mirroring_source_id.empty());
  EXPECT_TRUE(result[0]->is_enabled);

  ASSERT_TRUE(base::StringToInt64(result[1]->id, &display_id))
      << "Display id must be convertable to integer: " << result[0]->id;

  ASSERT_TRUE(DisplayExists(display_id)) << display_id << " not found";
  EXPECT_EQ(GetDisplayManager()->GetDisplayNameForId(display_id),
            result[1]->name);

  // Initial multipple display configuration.
  EXPECT_EQ("500,0 400x520",
            SystemInfoDisplayBoundsToString(result[1]->bounds));
  EXPECT_EQ("0,0,0,0", SystemInfoDisplayInsetsToString(result[1]->overscan));
  EXPECT_EQ(0, result[1]->rotation);
  EXPECT_FALSE(result[1]->is_primary);
  EXPECT_EQ(96, result[1]->dpi_x);
  EXPECT_EQ(96, result[1]->dpi_y);
  EXPECT_TRUE(result[1]->mirroring_source_id.empty());
  EXPECT_TRUE(result[1]->is_enabled);

  // Enable unified.
  GetDisplayManager()->SetUnifiedDesktopEnabled(true);
  EXPECT_TRUE(GetDisplayManager()->IsInUnifiedMode());

  result = DisplayInfoProvider::Get()->GetAllDisplaysInfo();

  ASSERT_EQ(2u, result.size());

  ASSERT_TRUE(base::StringToInt64(result[0]->id, &display_id))
      << "Display id must be convertable to integer: " << result[0]->id;

  EXPECT_EQ("0,0 500x600", SystemInfoDisplayBoundsToString(result[0]->bounds));
  EXPECT_EQ("0,0,0,0", SystemInfoDisplayInsetsToString(result[0]->overscan));
  EXPECT_EQ(0, result[0]->rotation);
  EXPECT_TRUE(result[0]->is_primary);
  EXPECT_EQ(96, result[0]->dpi_x);
  EXPECT_EQ(96, result[0]->dpi_y);
  EXPECT_TRUE(result[0]->mirroring_source_id.empty());
  EXPECT_TRUE(result[0]->is_enabled);

  ASSERT_TRUE(base::StringToInt64(result[1]->id, &display_id))
      << "Display id must be convertable to integer: " << result[0]->id;

  EXPECT_EQ(GetDisplayManager()->GetDisplayNameForId(display_id),
            result[1]->name);

  // After enabling unified the second display is scaled to meet the height for
  // the first. Which also affects the DPI below.
  EXPECT_EQ("500,0 461x600",
            SystemInfoDisplayBoundsToString(result[1]->bounds));
  EXPECT_EQ("0,0,0,0", SystemInfoDisplayInsetsToString(result[1]->overscan));
  EXPECT_EQ(0, result[1]->rotation);
  EXPECT_FALSE(result[1]->is_primary);
  EXPECT_FLOAT_EQ(111, round(result[1]->dpi_x));
  EXPECT_FLOAT_EQ(111, round(result[1]->dpi_y));
  EXPECT_TRUE(result[1]->mirroring_source_id.empty());
  EXPECT_TRUE(result[1]->is_enabled);

  // Disable unified and check that once again it matches initial situation.
  GetDisplayManager()->SetUnifiedDesktopEnabled(false);
  EXPECT_FALSE(GetDisplayManager()->IsInUnifiedMode());
  result = DisplayInfoProvider::Get()->GetAllDisplaysInfo();

  ASSERT_EQ(2u, result.size());

  ASSERT_TRUE(base::StringToInt64(result[0]->id, &display_id))
      << "Display id must be convertable to integer: " << result[0]->id;

  ASSERT_TRUE(DisplayExists(display_id)) << display_id << " not found";
  EXPECT_EQ("0,0 500x600", SystemInfoDisplayBoundsToString(result[0]->bounds));
  EXPECT_EQ("0,0,0,0", SystemInfoDisplayInsetsToString(result[0]->overscan));
  EXPECT_EQ(0, result[0]->rotation);
  EXPECT_TRUE(result[0]->is_primary);
  EXPECT_EQ(96, result[0]->dpi_x);
  EXPECT_EQ(96, result[0]->dpi_y);
  EXPECT_TRUE(result[0]->mirroring_source_id.empty());
  EXPECT_TRUE(result[0]->is_enabled);

  ASSERT_TRUE(base::StringToInt64(result[1]->id, &display_id))
      << "Display id must be convertable to integer: " << result[0]->id;

  ASSERT_TRUE(DisplayExists(display_id)) << display_id << " not found";
  EXPECT_EQ(GetDisplayManager()->GetDisplayNameForId(display_id),
            result[1]->name);
  EXPECT_EQ("500,0 400x520",
            SystemInfoDisplayBoundsToString(result[1]->bounds));
  EXPECT_EQ("0,0,0,0", SystemInfoDisplayInsetsToString(result[1]->overscan));
  EXPECT_EQ(0, result[1]->rotation);
  EXPECT_FALSE(result[1]->is_primary);
  EXPECT_EQ(96, result[1]->dpi_x);
  EXPECT_EQ(96, result[1]->dpi_y);
  EXPECT_TRUE(result[1]->mirroring_source_id.empty());
  EXPECT_TRUE(result[1]->is_enabled);
}

TEST_F(DisplayInfoProviderChromeosTest, GetRotation) {
  UpdateDisplay("500x600/r");
  DisplayInfo result = DisplayInfoProvider::Get()->GetAllDisplaysInfo();

  ASSERT_EQ(1u, result.size());

  int64_t display_id;
  ASSERT_TRUE(base::StringToInt64(result[0]->id, &display_id))
      << "Display id must be convertable to integer: " << result[0]->id;

  ASSERT_TRUE(DisplayExists(display_id)) << display_id << " not found";
  EXPECT_EQ("0,0 600x500", SystemInfoDisplayBoundsToString(result[0]->bounds));
  EXPECT_EQ(90, result[0]->rotation);

  GetDisplayManager()->SetDisplayRotation(display_id, gfx::Display::ROTATE_270,
                                          gfx::Display::ROTATION_SOURCE_ACTIVE);

  result = DisplayInfoProvider::Get()->GetAllDisplaysInfo();

  ASSERT_EQ(1u, result.size());

  EXPECT_EQ(base::Int64ToString(display_id), result[0]->id);
  EXPECT_EQ("0,0 600x500", SystemInfoDisplayBoundsToString(result[0]->bounds));
  EXPECT_EQ(270, result[0]->rotation);

  GetDisplayManager()->SetDisplayRotation(display_id, gfx::Display::ROTATE_180,
                                          gfx::Display::ROTATION_SOURCE_ACTIVE);

  result = DisplayInfoProvider::Get()->GetAllDisplaysInfo();

  ASSERT_EQ(1u, result.size());

  EXPECT_EQ(base::Int64ToString(display_id), result[0]->id);
  EXPECT_EQ("0,0 500x600", SystemInfoDisplayBoundsToString(result[0]->bounds));
  EXPECT_EQ(180, result[0]->rotation);

  GetDisplayManager()->SetDisplayRotation(display_id, gfx::Display::ROTATE_0,
                                          gfx::Display::ROTATION_SOURCE_ACTIVE);

  result = DisplayInfoProvider::Get()->GetAllDisplaysInfo();

  ASSERT_EQ(1u, result.size());

  EXPECT_EQ(base::Int64ToString(display_id), result[0]->id);
  EXPECT_EQ("0,0 500x600", SystemInfoDisplayBoundsToString(result[0]->bounds));
  EXPECT_EQ(0, result[0]->rotation);
}

TEST_F(DisplayInfoProviderChromeosTest, GetDPI) {
  UpdateDisplay("500x600,400x520*2");
  DisplayInfo result;
  result = DisplayInfoProvider::Get()->GetAllDisplaysInfo();

  ASSERT_EQ(2u, result.size());

  EXPECT_EQ("0,0 500x600", SystemInfoDisplayBoundsToString(result[0]->bounds));
  EXPECT_EQ(96, result[0]->dpi_x);
  EXPECT_EQ(96, result[0]->dpi_y);

  EXPECT_EQ("500,0 200x260",
            SystemInfoDisplayBoundsToString(result[1]->bounds));
  // DPI should be 96 (native dpi) * 200 (display) / 400 (native) when ui scale
  // is 2.
  EXPECT_EQ(96 / 2, result[1]->dpi_x);
  EXPECT_EQ(96 / 2, result[1]->dpi_y);

  ash::test::SwapPrimaryDisplay();

  result = DisplayInfoProvider::Get()->GetAllDisplaysInfo();

  ASSERT_EQ(2u, result.size());

  EXPECT_EQ("-500,0 500x600",
            SystemInfoDisplayBoundsToString(result[0]->bounds));
  EXPECT_EQ(96, result[0]->dpi_x);
  EXPECT_EQ(96, result[0]->dpi_y);

  EXPECT_EQ("0,0 200x260", SystemInfoDisplayBoundsToString(result[1]->bounds));
  EXPECT_EQ(96 / 2, result[1]->dpi_x);
  EXPECT_EQ(96 / 2, result[1]->dpi_y);
}

TEST_F(DisplayInfoProviderChromeosTest, GetVisibleArea) {
  UpdateDisplay("640x720*2/o, 400x520/o");
  DisplayInfo result;
  result = DisplayInfoProvider::Get()->GetAllDisplaysInfo();

  ASSERT_EQ(2u, result.size());

  int64_t display_id;
  ASSERT_TRUE(base::StringToInt64(result[1]->id, &display_id))
      << "Display id must be convertable to integer: " << result[1]->id;
  ASSERT_TRUE(DisplayExists(display_id)) << display_id << " not found";

  // Default overscan is 5%.
  EXPECT_EQ("304,0 380x494",
            SystemInfoDisplayBoundsToString(result[1]->bounds));
  EXPECT_EQ("13,10,13,10",
            SystemInfoDisplayInsetsToString(result[1]->overscan));

  GetDisplayManager()->SetOverscanInsets(display_id,
                                         gfx::Insets(20, 30, 50, 60));
  result = DisplayInfoProvider::Get()->GetAllDisplaysInfo();

  ASSERT_EQ(2u, result.size());

  EXPECT_EQ(base::Int64ToString(display_id), result[1]->id);
  EXPECT_EQ("304,0 310x450",
            SystemInfoDisplayBoundsToString(result[1]->bounds));
  EXPECT_EQ("20,30,50,60",
            SystemInfoDisplayInsetsToString(result[1]->overscan));

  // Set insets for the primary screen. Note that it has 2x scale.
  ASSERT_TRUE(base::StringToInt64(result[0]->id, &display_id))
      << "Display id must be convertable to integer: " << result[0]->id;
  ASSERT_TRUE(DisplayExists(display_id)) << display_id << " not found";

  EXPECT_EQ("0,0 304x342", SystemInfoDisplayBoundsToString(result[0]->bounds));
  EXPECT_EQ("9,8,9,8", SystemInfoDisplayInsetsToString(result[0]->overscan));

  GetDisplayManager()->SetOverscanInsets(display_id,
                                         gfx::Insets(10, 20, 30, 40));
  result = DisplayInfoProvider::Get()->GetAllDisplaysInfo();

  ASSERT_EQ(2u, result.size());

  EXPECT_EQ(base::Int64ToString(display_id), result[0]->id);
  EXPECT_EQ("0,0 260x320", SystemInfoDisplayBoundsToString(result[0]->bounds));
  EXPECT_EQ("10,20,30,40",
            SystemInfoDisplayInsetsToString(result[0]->overscan));
}

TEST_F(DisplayInfoProviderChromeosTest, GetMirroring) {
  UpdateDisplay("600x600, 400x520/o");
  DisplayInfo result;
  result = DisplayInfoProvider::Get()->GetAllDisplaysInfo();

  ASSERT_EQ(2u, result.size());

  int64_t display_id_primary;
  ASSERT_TRUE(base::StringToInt64(result[0]->id, &display_id_primary))
      << "Display id must be convertable to integer: " << result[0]->id;
  ASSERT_TRUE(DisplayExists(display_id_primary)) << display_id_primary
                                                 << " not found";

  int64_t display_id_secondary;
  ASSERT_TRUE(base::StringToInt64(result[1]->id, &display_id_secondary))
      << "Display id must be convertable to integer: " << result[1]->id;
  ASSERT_TRUE(DisplayExists(display_id_secondary)) << display_id_secondary
                                                   << " not found";

  ASSERT_FALSE(GetDisplayManager()->IsInMirrorMode());
  EXPECT_TRUE(result[0]->mirroring_source_id.empty());
  EXPECT_TRUE(result[1]->mirroring_source_id.empty());

  GetDisplayManager()->SetMirrorMode(true);
  ASSERT_TRUE(GetDisplayManager()->IsInMirrorMode());

  result = DisplayInfoProvider::Get()->GetAllDisplaysInfo();

  ASSERT_EQ(1u, result.size());
  EXPECT_EQ(base::Int64ToString(display_id_primary), result[0]->id);
  EXPECT_EQ(base::Int64ToString(display_id_secondary),
            result[0]->mirroring_source_id);

  GetDisplayManager()->SetMirrorMode(false);
  ASSERT_FALSE(GetDisplayManager()->IsInMirrorMode());

  result = DisplayInfoProvider::Get()->GetAllDisplaysInfo();

  ASSERT_EQ(2u, result.size());
  EXPECT_EQ(base::Int64ToString(display_id_primary), result[0]->id);
  EXPECT_TRUE(result[0]->mirroring_source_id.empty());
  EXPECT_EQ(base::Int64ToString(display_id_secondary), result[1]->id);
  EXPECT_TRUE(result[1]->mirroring_source_id.empty());
}

TEST_F(DisplayInfoProviderChromeosTest, GetBounds) {
  UpdateDisplay("600x600, 400x520");
  GetDisplayManager()->SetLayoutForCurrentDisplays(
      ash::DisplayLayout(ash::DisplayPlacement::LEFT, -40));

  DisplayInfo result = DisplayInfoProvider::Get()->GetAllDisplaysInfo();

  ASSERT_EQ(2u, result.size());
  EXPECT_EQ("0,0 600x600", SystemInfoDisplayBoundsToString(result[0]->bounds));
  EXPECT_EQ("-400,-40 400x520",
            SystemInfoDisplayBoundsToString(result[1]->bounds));

  GetDisplayManager()->SetLayoutForCurrentDisplays(
      ash::DisplayLayout(ash::DisplayPlacement::TOP, 40));

  result = DisplayInfoProvider::Get()->GetAllDisplaysInfo();

  ASSERT_EQ(2u, result.size());
  EXPECT_EQ("0,0 600x600", SystemInfoDisplayBoundsToString(result[0]->bounds));
  EXPECT_EQ("40,-520 400x520",
            SystemInfoDisplayBoundsToString(result[1]->bounds));

  GetDisplayManager()->SetLayoutForCurrentDisplays(
      ash::DisplayLayout(ash::DisplayPlacement::BOTTOM, 80));

  result = DisplayInfoProvider::Get()->GetAllDisplaysInfo();
  ASSERT_EQ(2u, result.size());
  EXPECT_EQ("0,0 600x600", SystemInfoDisplayBoundsToString(result[0]->bounds));
  EXPECT_EQ("80,600 400x520",
            SystemInfoDisplayBoundsToString(result[1]->bounds));
}

TEST_F(DisplayInfoProviderChromeosTest, SetBoundsOriginLeftExact) {
  UpdateDisplay("1200x600,520x400");

  const gfx::Display& secondary = ash::ScreenUtil::GetSecondaryDisplay();
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

  const gfx::Display& secondary = ash::ScreenUtil::GetSecondaryDisplay();
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

  const gfx::Display& secondary = ash::ScreenUtil::GetSecondaryDisplay();
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

  const gfx::Display& secondary = ash::ScreenUtil::GetSecondaryDisplay();
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

  const gfx::Display& secondary = ash::ScreenUtil::GetSecondaryDisplay();
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

  const gfx::Display& secondary = ash::ScreenUtil::GetSecondaryDisplay();
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

  const gfx::Display& secondary = ash::ScreenUtil::GetSecondaryDisplay();
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

  const gfx::Display& secondary = ash::ScreenUtil::GetSecondaryDisplay();
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

  const gfx::Display& secondary = ash::ScreenUtil::GetSecondaryDisplay();
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

  const gfx::Display& secondary = ash::ScreenUtil::GetSecondaryDisplay();
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

  const gfx::Display& secondary = ash::ScreenUtil::GetSecondaryDisplay();
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

  const gfx::Display& secondary = ash::ScreenUtil::GetSecondaryDisplay();
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

  const gfx::Display& secondary = ash::ScreenUtil::GetSecondaryDisplay();
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

  const gfx::Display& secondary = ash::ScreenUtil::GetSecondaryDisplay();
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

  const gfx::Display& secondary = ash::ScreenUtil::GetSecondaryDisplay();
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

  const gfx::Display& secondary = ash::ScreenUtil::GetSecondaryDisplay();
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

  const gfx::Display& secondary = ash::ScreenUtil::GetSecondaryDisplay();
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
  EXPECT_NE(gfx::Screen::GetScreen()->GetPrimaryDisplay().id(), secondary.id());
}

TEST_F(DisplayInfoProviderChromeosTest, SetBoundsOriginWithMirroring) {
  UpdateDisplay("1200x600,600x1000*2");

  const gfx::Display& secondary = ash::ScreenUtil::GetSecondaryDisplay();
  const gfx::Display& primary = gfx::Screen::GetScreen()->GetPrimaryDisplay();

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

  const gfx::Display& secondary = ash::ScreenUtil::GetSecondaryDisplay();
  api::system_display::DisplayProperties info;
  info.rotation.reset(new int(90));

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_TRUE(success);
  EXPECT_TRUE(error.empty());

  EXPECT_EQ("1200,0 500x300", secondary.bounds().ToString());
  EXPECT_EQ(gfx::Display::ROTATE_90, secondary.rotation());

  info.rotation.reset(new int(270));
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_TRUE(success);
  EXPECT_TRUE(error.empty());

  EXPECT_EQ("1200,0 500x300", secondary.bounds().ToString());
  EXPECT_EQ(gfx::Display::ROTATE_270, secondary.rotation());

  info.rotation.reset(new int(180));
  // Switch primary display.
  info.is_primary.reset(new bool(true));
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_TRUE(success);
  EXPECT_TRUE(error.empty());

  EXPECT_EQ("0,0 300x500", secondary.bounds().ToString());
  EXPECT_EQ(gfx::Display::ROTATE_180, secondary.rotation());
  EXPECT_EQ(gfx::Screen::GetScreen()->GetPrimaryDisplay().id(), secondary.id());

  info.rotation.reset(new int(0));
  CallSetDisplayUnitInfo(
      base::Int64ToString(secondary.id()), info, &success, &error);

  ASSERT_TRUE(success);
  EXPECT_TRUE(error.empty());

  EXPECT_EQ("0,0 300x500", secondary.bounds().ToString());
  EXPECT_EQ(gfx::Display::ROTATE_0, secondary.rotation());
  EXPECT_EQ(gfx::Screen::GetScreen()->GetPrimaryDisplay().id(), secondary.id());
}

// Tests that rotation changes made before entering maximize mode are restored
// upon exiting maximize mode, and that a rotation lock is not set.
TEST_F(DisplayInfoProviderChromeosTest, SetRotationBeforeMaximizeMode) {
  ash::ScreenOrientationController* screen_orientation_controller =
      ash::Shell::GetInstance()->screen_orientation_controller();
  api::system_display::DisplayProperties info;
  info.rotation.reset(new int(90));

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(base::Int64ToString(gfx::Display::InternalDisplayId()),
                         info, &success, &error);

  ASSERT_TRUE(success);
  EXPECT_TRUE(error.empty());
  EXPECT_FALSE(screen_orientation_controller->rotation_locked());

  // Entering maximize mode enables accelerometer screen rotations.
  ash::Shell::GetInstance()
      ->maximize_mode_controller()
      ->EnableMaximizeModeWindowManager(true);
  // Rotation lock should not activate because DisplayInfoProvider::SetInfo()
  // was called when not in maximize mode.
  EXPECT_FALSE(screen_orientation_controller->rotation_locked());

  // ScreenOrientationController rotations override display info.
  screen_orientation_controller->SetDisplayRotation(
      gfx::Display::ROTATE_0, gfx::Display::ROTATION_SOURCE_ACTIVE);
  EXPECT_EQ(gfx::Display::ROTATE_0, GetCurrentInternalDisplayRotation());

  // Exiting maximize mode should restore the initial rotation
  ash::Shell::GetInstance()
      ->maximize_mode_controller()
      ->EnableMaximizeModeWindowManager(false);
  EXPECT_EQ(gfx::Display::ROTATE_90, GetCurrentInternalDisplayRotation());
}

// Tests that rotation changes made during maximize mode lock the display
// against accelerometer rotations.
TEST_F(DisplayInfoProviderChromeosTest, SetRotationDuringMaximizeMode) {
  // Entering maximize mode enables accelerometer screen rotations.
  ash::Shell::GetInstance()
      ->maximize_mode_controller()
      ->EnableMaximizeModeWindowManager(true);

  ASSERT_FALSE(ash::Shell::GetInstance()
                   ->screen_orientation_controller()
                   ->rotation_locked());

  api::system_display::DisplayProperties info;
  info.rotation.reset(new int(90));

  bool success = false;
  std::string error;
  CallSetDisplayUnitInfo(base::Int64ToString(gfx::Display::InternalDisplayId()),
                         info, &success, &error);

  ASSERT_TRUE(success);
  EXPECT_TRUE(error.empty());
  EXPECT_TRUE(ash::Shell::GetInstance()
                  ->screen_orientation_controller()
                  ->rotation_locked());
}

TEST_F(DisplayInfoProviderChromeosTest, SetInvalidRotation) {
  UpdateDisplay("1200x600,600x1000*2");

  const gfx::Display& secondary = ash::ScreenUtil::GetSecondaryDisplay();
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

  const gfx::Display& secondary = ash::ScreenUtil::GetSecondaryDisplay();
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

  const gfx::Display& secondary = ash::ScreenUtil::GetSecondaryDisplay();
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

  const gfx::Display& secondary = ash::ScreenUtil::GetSecondaryDisplay();
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

  const gfx::Display& secondary = ash::ScreenUtil::GetSecondaryDisplay();
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
      ash::test::DisplayManagerTestApi().SetFirstDisplayAsInternalDisplay();

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

}  // namespace
}  // namespace extensions
