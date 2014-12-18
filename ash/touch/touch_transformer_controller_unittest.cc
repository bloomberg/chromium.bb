// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_transformer_controller.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/test/events_test_utils_x11.h"
#include "ui/gfx/display.h"

namespace ash {

namespace {

DisplayInfo CreateDisplayInfo(int64 id,
                              unsigned int touch_device_id,
                              const gfx::Rect& bounds) {
  DisplayInfo info(id, std::string(), false);
  info.SetBounds(bounds);
  info.set_touch_device_id(touch_device_id);

  // Create a default mode.
  std::vector<DisplayMode> default_modes(
      1, DisplayMode(bounds.size(), 60, false, true));
  info.SetDisplayModes(default_modes);

  return info;
}

ui::TouchscreenDevice CreateTouchscreenDevice(unsigned int id,
                                              const gfx::Size& size) {
  return ui::TouchscreenDevice(id,
                               ui::InputDeviceType::INPUT_DEVICE_EXTERNAL,
                               std::string(),
                               size);
}

}  //  namespace

typedef test::AshTestBase TouchTransformerControllerTest;

TEST_F(TouchTransformerControllerTest, TouchTransformerMirrorModeLetterboxing) {
  // The internal display has native resolution of 2560x1700, and in
  // mirror mode it is configured as 1920x1200. This is in letterboxing
  // mode.
  DisplayInfo internal_display_info =
      CreateDisplayInfo(1, 10u, gfx::Rect(0, 0, 1920, 1200));
  internal_display_info.set_is_aspect_preserving_scaling(true);
  std::vector<DisplayMode> internal_modes;
  internal_modes.push_back(
      DisplayMode(gfx::Size(2560, 1700), 60, false, true));
  internal_modes.push_back(
      DisplayMode(gfx::Size(1920, 1200), 60, false, false));
  internal_display_info.SetDisplayModes(internal_modes);

  DisplayInfo external_display_info =
      CreateDisplayInfo(2, 11u, gfx::Rect(0, 0, 1920, 1200));

  gfx::Size fb_size(1920, 1200);

  // Create the touchscreens with the same size as the framebuffer so we can
  // share the tests between Ozone & X11.
  ui::TouchscreenDevice internal_touchscreen =
      CreateTouchscreenDevice(10, fb_size);
  ui::TouchscreenDevice external_touchscreen =
      CreateTouchscreenDevice(11, fb_size);

  TouchTransformerController* tt_controller =
      Shell::GetInstance()->touch_transformer_controller();
  ui::DeviceDataManager* device_manager = ui::DeviceDataManager::GetInstance();

  device_manager->UpdateTouchInfoForDisplay(
      internal_display_info.id(),
      internal_display_info.touch_device_id(),
      tt_controller->GetTouchTransform(internal_display_info,
                                       internal_touchscreen,
                                       fb_size));

  device_manager->UpdateTouchInfoForDisplay(
      external_display_info.id(),
      external_display_info.touch_device_id(),
      tt_controller->GetTouchTransform(external_display_info,
                                       external_touchscreen,
                                       fb_size));

  EXPECT_EQ(1, device_manager->GetDisplayForTouchDevice(10));
  EXPECT_EQ(2, device_manager->GetDisplayForTouchDevice(11));

  // External touch display has the default TouchTransformer.
  float x = 100.0;
  float y = 100.0;
  device_manager->ApplyTouchTransformer(11, &x, &y);
  EXPECT_EQ(100, x);
  EXPECT_EQ(100, y);

  // In letterboxing, there is (1-2560*(1200/1920)/1700)/2 = 2.95% of the
  // height on both the top & bottom region of the screen is blank.
  // When touch events coming at Y range [0, 1200), the mapping should be
  // [0, ~35] ---> < 0
  // [~35, ~1165] ---> [0, 1200)
  // [~1165, 1200] ---> >= 1200
  x = 100.0;
  y = 35.0;
  device_manager->ApplyTouchTransformer(10, &x, &y);
  EXPECT_EQ(100, static_cast<int>(x));
  EXPECT_EQ(0, static_cast<int>(y));

  x = 100.0;
  y = 1165.0;
  device_manager->ApplyTouchTransformer(10, &x, &y);
  EXPECT_EQ(100, static_cast<int>(x));
  EXPECT_EQ(1200, static_cast<int>(y));
}

TEST_F(TouchTransformerControllerTest, TouchTransformerMirrorModePillarboxing) {
  // The internal display has native resolution of 1366x768, and in
  // mirror mode it is configured as 1024x768. This is in pillarboxing
  // mode.
  DisplayInfo internal_display_info =
      CreateDisplayInfo(1, 10, gfx::Rect(0, 0, 1024, 768));
  internal_display_info.set_is_aspect_preserving_scaling(true);
  std::vector<DisplayMode> internal_modes;
  internal_modes.push_back(
      DisplayMode(gfx::Size(1366, 768), 60, false, true));
  internal_modes.push_back(
      DisplayMode(gfx::Size(1024, 768), 60, false, false));
  internal_display_info.SetDisplayModes(internal_modes);

  DisplayInfo external_display_info =
      CreateDisplayInfo(2, 11, gfx::Rect(0, 0, 1024, 768));

  gfx::Size fb_size(1024, 768);

  // Create the touchscreens with the same size as the framebuffer so we can
  // share the tests between Ozone & X11.
  ui::TouchscreenDevice internal_touchscreen =
      CreateTouchscreenDevice(10, fb_size);
  ui::TouchscreenDevice external_touchscreen =
      CreateTouchscreenDevice(11, fb_size);

  TouchTransformerController* tt_controller =
      Shell::GetInstance()->touch_transformer_controller();
  ui::DeviceDataManager* device_manager = ui::DeviceDataManager::GetInstance();

  device_manager->UpdateTouchInfoForDisplay(
      internal_display_info.id(),
      internal_display_info.touch_device_id(),
      tt_controller->GetTouchTransform(internal_display_info,
                                       internal_touchscreen,
                                       fb_size));

  device_manager->UpdateTouchInfoForDisplay(
      external_display_info.id(),
      external_display_info.touch_device_id(),
      tt_controller->GetTouchTransform(external_display_info,
                                       external_touchscreen,
                                       fb_size));

  EXPECT_EQ(1, device_manager->GetDisplayForTouchDevice(10));
  EXPECT_EQ(2, device_manager->GetDisplayForTouchDevice(11));

  // External touch display has the default TouchTransformer.
  float x = 100.0;
  float y = 100.0;
  device_manager->ApplyTouchTransformer(11, &x, &y);
  EXPECT_EQ(100, x);
  EXPECT_EQ(100, y);

  // In pillarboxing, there is (1-768*(1024/768)/1366)/2 = 12.5% of the
  // width on both the left & rigth region of the screen is blank.
  // When touch events coming at X range [0, 1024), the mapping should be
  // [0, ~128] ---> < 0
  // [~128, ~896] ---> [0, 1024)
  // [~896, 1024] ---> >= 1024
  x = 128.0;
  y = 100.0;
  device_manager->ApplyTouchTransformer(10, &x, &y);
  EXPECT_EQ(0, static_cast<int>(x));
  EXPECT_EQ(100, static_cast<int>(y));

  x = 896.0;
  y = 100.0;
  device_manager->ApplyTouchTransformer(10, &x, &y);
  EXPECT_EQ(1024, static_cast<int>(x));
  EXPECT_EQ(100, static_cast<int>(y));
}

TEST_F(TouchTransformerControllerTest, TouchTransformerExtendedMode) {
  // The internal display has size 1366 x 768. The external display has
  // size 2560x1600. The total frame buffer is 2560x2428,
  // where 2428 = 768 + 60 (hidden gap) + 1600
  // and the sceond monitor is translated to Point (0, 828) in the
  // framebuffer.
  DisplayInfo display1 = CreateDisplayInfo(1, 5u, gfx::Rect(0, 0, 1366, 768));
  DisplayInfo display2 =
      CreateDisplayInfo(2, 6u, gfx::Rect(0, 828, 2560, 1600));
  gfx::Size fb_size(2560, 2428);

  // Create the touchscreens with the same size as the framebuffer so we can
  // share the tests between Ozone & X11.
  ui::TouchscreenDevice touchscreen1 = CreateTouchscreenDevice(5, fb_size);
  ui::TouchscreenDevice touchscreen2 = CreateTouchscreenDevice(6, fb_size);

  TouchTransformerController* tt_controller =
      Shell::GetInstance()->touch_transformer_controller();
  ui::DeviceDataManager* device_manager = ui::DeviceDataManager::GetInstance();

  device_manager->UpdateTouchInfoForDisplay(
      display1.id(),
      display1.touch_device_id(),
      tt_controller->GetTouchTransform(display1,
                                       touchscreen1,
                                       fb_size));

  device_manager->UpdateTouchInfoForDisplay(
      display2.id(),
      display2.touch_device_id(),
      tt_controller->GetTouchTransform(display2,
                                       touchscreen2,
                                       fb_size));

  EXPECT_EQ(1, device_manager->GetDisplayForTouchDevice(5));
  EXPECT_EQ(2, device_manager->GetDisplayForTouchDevice(6));

  // Mapping for touch events from internal touch display:
  // [0, 2560) x [0, 2428) -> [0, 1366) x [0, 768)
  float x = 0.0;
  float y = 0.0;
  device_manager->ApplyTouchTransformer(5, &x, &y);
  EXPECT_EQ(0, static_cast<int>(x));
  EXPECT_EQ(0, static_cast<int>(y));

  x = 2559.0;
  y = 2427.0;
  device_manager->ApplyTouchTransformer(5, &x, &y);
  EXPECT_EQ(1365, static_cast<int>(x));
  EXPECT_EQ(767, static_cast<int>(y));

  // Mapping for touch events from external touch display:
  // [0, 2560) x [0, 2428) -> [0, 2560) x [0, 1600)
  x = 0.0;
  y = 0.0;
  device_manager->ApplyTouchTransformer(6, &x, &y);
#if defined(USE_OZONE)
  // On ozone we expect screen coordinates so add display origin.
  EXPECT_EQ(0 + 0, static_cast<int>(x));
  EXPECT_EQ(0 + 828, static_cast<int>(y));
#else
  EXPECT_EQ(0, static_cast<int>(x));
  EXPECT_EQ(0, static_cast<int>(y));
#endif

  x = 2559.0;
  y = 2427.0;
  device_manager->ApplyTouchTransformer(6, &x, &y);
#if defined(USE_OZONE)
  // On ozone we expect screen coordinates so add display origin.
  EXPECT_EQ(2559 + 0, static_cast<int>(x));
  EXPECT_EQ(1599 + 828, static_cast<int>(y));
#else
  EXPECT_EQ(2559, static_cast<int>(x));
  EXPECT_EQ(1599, static_cast<int>(y));
#endif
}

TEST_F(TouchTransformerControllerTest, TouchRadiusScale) {
  DisplayInfo display = CreateDisplayInfo(1, 5u, gfx::Rect(0, 0, 2560, 1600));
  ui::TouchscreenDevice touch_device =
      CreateTouchscreenDevice(5, gfx::Size(1001, 1001));

  TouchTransformerController* tt_controller =
      Shell::GetInstance()->touch_transformer_controller();
  // Default touchscreen position range is 1001x1001;
  EXPECT_EQ(sqrt((2560.0 * 1600.0) / (1001.0 * 1001.0)),
            tt_controller->GetTouchResolutionScale(display, touch_device));
}

}  // namespace ash
