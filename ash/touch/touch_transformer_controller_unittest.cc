// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_transformer_controller.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/devices/device_data_manager.h"

namespace ash {

namespace {

DisplayInfo CreateDisplayInfo(int64_t id,
                              unsigned int touch_device_id,
                              const gfx::Rect& bounds) {
  DisplayInfo info(id, std::string(), false);
  info.SetBounds(bounds);
  info.AddInputDevice(touch_device_id);

  // Create a default mode.
  std::vector<DisplayMode> default_modes(
      1, DisplayMode(bounds.size(), 60, false, true));
  info.SetDisplayModes(default_modes);

  return info;
}

ui::TouchscreenDevice CreateTouchscreenDevice(unsigned int id,
                                              const gfx::Size& size) {
  return ui::TouchscreenDevice(id, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL,
                               std::string(), size, 0);
}

}  //  namespace

typedef test::AshTestBase TouchTransformerControllerTest;

TEST_F(TouchTransformerControllerTest, MirrorModeLetterboxing) {
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
      internal_display_info.id(), internal_touchscreen.id,
      tt_controller->GetTouchTransform(internal_display_info,
                                       internal_display_info,
                                       internal_touchscreen, fb_size));

  device_manager->UpdateTouchInfoForDisplay(
      internal_display_info.id(), external_touchscreen.id,
      tt_controller->GetTouchTransform(external_display_info,
                                       external_display_info,
                                       external_touchscreen, fb_size));

  EXPECT_EQ(1, device_manager->GetTargetDisplayForTouchDevice(10));
  EXPECT_EQ(1, device_manager->GetTargetDisplayForTouchDevice(11));

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
  EXPECT_NEAR(100, x, 0.5);
  EXPECT_NEAR(0, y, 0.5);

  x = 100.0;
  y = 1165.0;
  device_manager->ApplyTouchTransformer(10, &x, &y);
  EXPECT_NEAR(100, x, 0.5);
  EXPECT_NEAR(1200, y, 0.5);
}

TEST_F(TouchTransformerControllerTest, MirrorModePillarboxing) {
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
      internal_display_info.id(), internal_touchscreen.id,
      tt_controller->GetTouchTransform(internal_display_info,
                                       internal_display_info,
                                       internal_touchscreen, fb_size));

  device_manager->UpdateTouchInfoForDisplay(
      internal_display_info.id(), external_touchscreen.id,
      tt_controller->GetTouchTransform(external_display_info,
                                       external_display_info,
                                       external_touchscreen, fb_size));

  EXPECT_EQ(1, device_manager->GetTargetDisplayForTouchDevice(10));
  EXPECT_EQ(1, device_manager->GetTargetDisplayForTouchDevice(11));

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
  EXPECT_NEAR(0, x, 0.5);
  EXPECT_NEAR(100, y, 0.5);

  x = 896.0;
  y = 100.0;
  device_manager->ApplyTouchTransformer(10, &x, &y);
  EXPECT_NEAR(1024, x, 0.5);
  EXPECT_NEAR(100, y, 0.5);
}

TEST_F(TouchTransformerControllerTest, SoftwareMirrorMode) {
  // External display 1 has size 1280x850. External display 2 has size
  // 1920x1080. When using software mirroring to mirror display 1 onto
  // display 2, the displays are in extended mode and we map touches from both
  // displays to display 1.
  // The total frame buffer is 1920x1990,
  // where 1990 = 850 + 60 (hidden gap) + 1080 and the second monitor is
  // translated to point (0, 950) in the framebuffer.
  DisplayInfo display1_info =
      CreateDisplayInfo(1, 10u, gfx::Rect(0, 0, 1280, 850));
  std::vector<DisplayMode> display1_modes;
  display1_modes.push_back(DisplayMode(gfx::Size(1280, 850), 60, false, true));
  display1_info.SetDisplayModes(display1_modes);

  DisplayInfo display2_info =
      CreateDisplayInfo(2, 11u, gfx::Rect(0, 950, 1920, 1080));
  std::vector<DisplayMode> display2_modes;
  display2_modes.push_back(DisplayMode(gfx::Size(1920, 1080), 60, false, true));
  display2_info.SetDisplayModes(display2_modes);

  gfx::Size fb_size(1920, 1990);

  // Create the touchscreens with the same size as the framebuffer so we can
  // share the tests between Ozone & X11.
  ui::TouchscreenDevice display1_touchscreen =
      CreateTouchscreenDevice(10, fb_size);
  ui::TouchscreenDevice display2_touchscreen =
      CreateTouchscreenDevice(11, fb_size);

  TouchTransformerController* tt_controller =
      Shell::GetInstance()->touch_transformer_controller();
  ui::DeviceDataManager* device_manager = ui::DeviceDataManager::GetInstance();

  device_manager->UpdateTouchInfoForDisplay(
      display1_info.id(), display1_touchscreen.id,
      tt_controller->GetTouchTransform(display1_info, display1_info,
                                       display1_touchscreen, fb_size));

  device_manager->UpdateTouchInfoForDisplay(
      display1_info.id(), display2_touchscreen.id,
      tt_controller->GetTouchTransform(display1_info, display2_info,
                                       display2_touchscreen, fb_size));

  EXPECT_EQ(1, device_manager->GetTargetDisplayForTouchDevice(10));
  EXPECT_EQ(1, device_manager->GetTargetDisplayForTouchDevice(11));

  // Mapping for touch events from display 1's touchscreen:
  // [0, 1920) x [0, 1990) -> [0, 1280) x [0, 850)
  float x = 0.0;
  float y = 0.0;
  device_manager->ApplyTouchTransformer(10, &x, &y);
  EXPECT_NEAR(0, x, 0.5);
  EXPECT_NEAR(0, y, 0.5);

  x = 1920.0;
  y = 1990.0;
  device_manager->ApplyTouchTransformer(10, &x, &y);
  EXPECT_NEAR(1280, x, 0.5);
  EXPECT_NEAR(850, y, 0.5);

  // In pillarboxing, there is (1-1280*(1080/850)/1920)/2 = 7.65% of the
  // width on both the left & right region of the screen is blank.
  // Events come in the range [0, 1920) x [0, 1990).
  //
  // X mapping:
  // [0, ~147] ---> < 0
  // [~147, ~1773] ---> [0, 1280)
  // [~1773, 1920] ---> >= 1280
  // Y mapping:
  // [0, 1990) -> [0, 1080)
  x = 147.0;
  y = 0.0;
  device_manager->ApplyTouchTransformer(11, &x, &y);
  EXPECT_NEAR(0, x, 0.5);
  EXPECT_NEAR(0, y, 0.5);

  x = 1773.0;
  y = 1990.0;
  device_manager->ApplyTouchTransformer(11, &x, &y);
  EXPECT_NEAR(1280, x, 0.5);
  EXPECT_NEAR(850, y, 0.5);
}

TEST_F(TouchTransformerControllerTest, ExtendedMode) {
  // The internal display has size 1366 x 768. The external display has
  // size 2560x1600. The total frame buffer is 2560x2428,
  // where 2428 = 768 + 60 (hidden gap) + 1600
  // and the second monitor is translated to Point (0, 828) in the
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
      display1.id(), touchscreen1.id,
      tt_controller->GetTouchTransform(display1, display1, touchscreen1,
                                       fb_size));

  device_manager->UpdateTouchInfoForDisplay(
      display2.id(), touchscreen2.id,
      tt_controller->GetTouchTransform(display2, display2, touchscreen2,
                                       fb_size));

  EXPECT_EQ(1, device_manager->GetTargetDisplayForTouchDevice(5));
  EXPECT_EQ(2, device_manager->GetTargetDisplayForTouchDevice(6));

  // Mapping for touch events from internal touch display:
  // [0, 2560) x [0, 2428) -> [0, 1366) x [0, 768)
  float x = 0.0;
  float y = 0.0;
  device_manager->ApplyTouchTransformer(5, &x, &y);
  EXPECT_NEAR(0, x, 0.5);
  EXPECT_NEAR(0, y, 0.5);

  x = 2559.0;
  y = 2427.0;
  device_manager->ApplyTouchTransformer(5, &x, &y);
  EXPECT_NEAR(1365, x, 0.5);
  EXPECT_NEAR(768, y, 0.5);

  // Mapping for touch events from external touch display:
  // [0, 2560) x [0, 2428) -> [0, 2560) x [0, 1600)
  x = 0.0;
  y = 0.0;
  device_manager->ApplyTouchTransformer(6, &x, &y);
#if defined(USE_OZONE)
  // On ozone we expect screen coordinates so add display origin.
  EXPECT_NEAR(0 + 0, x, 0.5);
  EXPECT_NEAR(0 + 828, y, 0.5);
#else
  EXPECT_NEAR(0, x, 0.5);
  EXPECT_NEAR(0, y, 0.5);
#endif

  x = 2559.0;
  y = 2427.0;
  device_manager->ApplyTouchTransformer(6, &x, &y);
#if defined(USE_OZONE)
  // On ozone we expect screen coordinates so add display origin.
  EXPECT_NEAR(2559 + 0, x, 0.5);
  EXPECT_NEAR(1599 + 828, y, 0.5);
#else
  EXPECT_NEAR(2559, x, 0.5);
  EXPECT_NEAR(1599, y, 0.5);
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
