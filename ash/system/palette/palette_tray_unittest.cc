// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/palette_tray.h"

#include "ash/ash_switches.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/config.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/shell_test_api.h"
#include "ash/system/palette/palette_utils.h"
#include "ash/system/palette/test_palette_delegate.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test_shell_delegate.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/events/event.h"
#include "ui/events/test/device_data_manager_test_api.h"
#include "ui/events/test/event_generator.h"

namespace ash {

class PaletteTrayTest : public AshTestBase {
 public:
  PaletteTrayTest() {}
  ~PaletteTrayTest() override {}

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAshEnablePaletteOnAllDisplays);

    AshTestBase::SetUp();

    palette_utils::SetHasStylusInputForTesting();

    Shell::RegisterLocalStatePrefs(pref_service_.registry());

    palette_tray_ =
        StatusAreaWidgetTestHelper::GetStatusAreaWidget()->palette_tray();
    test_api_ = base::MakeUnique<PaletteTray::TestApi>(palette_tray_);

    // Set the test palette delegate here, since this requires an instance of
    // shell to be available.
    ShellTestApi().SetPaletteDelegate(base::MakeUnique<TestPaletteDelegate>());
    // Initialize the palette tray again since this test requires information
    // from the palette delegate. (It was initialized without the delegate in
    // AshTestBase::SetUp()).
    palette_tray_->Initialize();
    palette_tray_->OnLocalStatePrefServiceInitialized(&pref_service_);
  }

  // Performs a tap on the palette tray button.
  void PerformTap() {
    ui::GestureEvent tap(0, 0, 0, base::TimeTicks(),
                         ui::GestureEventDetails(ui::ET_GESTURE_TAP));
    palette_tray_->PerformAction(tap);
  }

 protected:
  PaletteTray* palette_tray_ = nullptr;  // not owned
  TestingPrefServiceSimple pref_service_;

  std::unique_ptr<PaletteTray::TestApi> test_api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PaletteTrayTest);
};

// Verify the palette tray button exists and but is not visible initially.
TEST_F(PaletteTrayTest, PaletteTrayIsInvisible) {
  ASSERT_TRUE(palette_tray_);
  EXPECT_FALSE(palette_tray_->visible());
}

// Verify that if the has seen stylus pref is not set initially, the palette
// tray's touch event watcher should be active.
TEST_F(PaletteTrayTest, PaletteTrayStylusWatcherAlive) {
  ASSERT_FALSE(palette_tray_->visible());

  EXPECT_TRUE(test_api_->IsStylusWatcherActive());
}

// Verify if the has seen stylus pref is not set initially, the palette tray
// should become visible after seeing a stylus event.
TEST_F(PaletteTrayTest, PaletteTrayVisibleAfterStylusSeen) {
  ASSERT_FALSE(palette_tray_->visible());
  ASSERT_FALSE(pref_service_.GetBoolean(prefs::kHasSeenStylus));
  ASSERT_TRUE(test_api_->IsStylusWatcherActive());

  // Send a stylus event.
  GetEventGenerator().EnterPenPointerMode();
  GetEventGenerator().PressTouch();
  GetEventGenerator().ReleaseTouch();
  GetEventGenerator().ExitPenPointerMode();

  // Verify that the palette tray is now visible, the stylus event watcher is
  // inactive and that the has seen stylus pref is now set to true.
  EXPECT_TRUE(palette_tray_->visible());
  EXPECT_FALSE(test_api_->IsStylusWatcherActive());
}

// Verify if the has seen stylus pref is initially set, the palette tray is
// visible.
TEST_F(PaletteTrayTest, StylusSeenPrefInitiallySet) {
  ASSERT_FALSE(palette_tray_->visible());
  pref_service_.SetBoolean(prefs::kHasSeenStylus, true);

  EXPECT_TRUE(palette_tray_->visible());
  EXPECT_FALSE(test_api_->IsStylusWatcherActive());
}

// Verify taps on the palette tray button results in expected behaviour.
TEST_F(PaletteTrayTest, PaletteTrayWorkflow) {
  // Verify the palette tray button is not active, and the palette tray bubble
  // is not shown initially.
  EXPECT_FALSE(palette_tray_->is_active());
  EXPECT_FALSE(test_api_->GetTrayBubbleWrapper());

  // Verify that by tapping the palette tray button, the button will become
  // active and the palette tray bubble will be open.
  PerformTap();
  EXPECT_TRUE(palette_tray_->is_active());
  EXPECT_TRUE(test_api_->GetTrayBubbleWrapper());

  // Verify that activating a mode tool will close the palette tray bubble, but
  // leave the palette tray button active.
  test_api_->GetPaletteToolManager()->ActivateTool(
      PaletteToolId::LASER_POINTER);
  EXPECT_TRUE(test_api_->GetPaletteToolManager()->IsToolActive(
      PaletteToolId::LASER_POINTER));
  EXPECT_TRUE(palette_tray_->is_active());
  EXPECT_FALSE(test_api_->GetTrayBubbleWrapper());

  // Verify that tapping the palette tray while a tool is active will deactivate
  // the tool, and the palette tray button will not be active.
  PerformTap();
  EXPECT_FALSE(palette_tray_->is_active());
  EXPECT_FALSE(test_api_->GetPaletteToolManager()->IsToolActive(
      PaletteToolId::LASER_POINTER));

  // Verify that activating a action tool will close the palette tray bubble and
  // the palette tray button is will not be active.
  PerformTap();
  ASSERT_TRUE(test_api_->GetTrayBubbleWrapper());
  test_api_->GetPaletteToolManager()->ActivateTool(
      PaletteToolId::CAPTURE_SCREEN);
  EXPECT_FALSE(test_api_->GetPaletteToolManager()->IsToolActive(
      PaletteToolId::CAPTURE_SCREEN));
  // Wait for the tray bubble widget to close.
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(test_api_->GetTrayBubbleWrapper());
  EXPECT_FALSE(palette_tray_->is_active());
}

// Verify that the palette tray button and bubble are as expected when modes
// that can be deactivated without pressing the palette tray button (such as
// capture region) are deactivated.
TEST_F(PaletteTrayTest, ModeToolDeactivatedAutomatically) {
  // Open the palette tray with a tap.
  PerformTap();
  ASSERT_TRUE(palette_tray_->is_active());
  ASSERT_TRUE(test_api_->GetTrayBubbleWrapper());

  // Activate and deactivate the capture region tool.
  test_api_->GetPaletteToolManager()->ActivateTool(
      PaletteToolId::CAPTURE_REGION);
  ASSERT_TRUE(test_api_->GetPaletteToolManager()->IsToolActive(
      PaletteToolId::CAPTURE_REGION));
  test_api_->GetPaletteToolManager()->DeactivateTool(
      PaletteToolId::CAPTURE_REGION);

  // Verify the bubble is hidden and the button is inactive after deactivating
  // the capture region tool.
  EXPECT_FALSE(test_api_->GetTrayBubbleWrapper());
  EXPECT_FALSE(palette_tray_->is_active());
}

// Base class for tests that need to simulate an internal stylus.
class PaletteTrayTestWithInternalStylus : public PaletteTrayTest {
 public:
  PaletteTrayTestWithInternalStylus() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kHasInternalStylus);
  }
  ~PaletteTrayTestWithInternalStylus() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(PaletteTrayTestWithInternalStylus);
};

// Verify the palette tray button exists and is visible if the device has an
// internal stylus.
TEST_F(PaletteTrayTestWithInternalStylus, Visible) {
  ASSERT_TRUE(palette_tray_);
  EXPECT_TRUE(palette_tray_->visible());
}

// Verify that when entering or exiting the lock screen, the behavior of the
// palette tray button is as expected.
TEST_F(PaletteTrayTestWithInternalStylus, PaletteTrayOnLockScreenBehavior) {
  ASSERT_TRUE(palette_tray_->visible());

  PaletteToolManager* manager = test_api_->GetPaletteToolManager();
  manager->ActivateTool(PaletteToolId::LASER_POINTER);
  EXPECT_TRUE(manager->IsToolActive(PaletteToolId::LASER_POINTER));

  // Verify that when entering the lock screen, the palette tray button is
  // hidden, and the tool that was active is no longer active.
  GetSessionControllerClient()->RequestLockScreen();
  EXPECT_FALSE(manager->IsToolActive(PaletteToolId::LASER_POINTER));
  EXPECT_FALSE(palette_tray_->visible());

  // Verify that when logging back in the tray is visible, but the tool that was
  // active before locking the screen is still inactive.
  GetSessionControllerClient()->UnlockScreen();
  EXPECT_TRUE(palette_tray_->visible());
  EXPECT_FALSE(manager->IsToolActive(PaletteToolId::LASER_POINTER));
}

}  // namespace ash
