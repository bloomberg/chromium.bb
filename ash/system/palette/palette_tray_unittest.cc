// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/palette_tray.h"

#include "ash/ash_switches.h"
#include "ash/shell_test_api.h"
#include "ash/system/palette/test_palette_delegate.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "ui/events/event.h"

namespace ash {

class PaletteTrayTest : public AshTestBase {
 public:
  PaletteTrayTest() {}
  ~PaletteTrayTest() override {}

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAshForceEnableStylusTools);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAshEnablePaletteOnAllDisplays);

    AshTestBase::SetUp();

    palette_tray_ =
        StatusAreaWidgetTestHelper::GetStatusAreaWidget()->palette_tray();
    test_api_ = base::MakeUnique<PaletteTray::TestApi>(palette_tray_);

    ShellTestApi().SetPaletteDelegate(base::MakeUnique<TestPaletteDelegate>());
  }

  // Performs a tap on the palette tray button.
  void PerformTap() {
    ui::GestureEvent tap(0, 0, 0, base::TimeTicks(),
                         ui::GestureEventDetails(ui::ET_GESTURE_TAP));
    palette_tray_->PerformAction(tap);
  }

 protected:
  PaletteTray* palette_tray_ = nullptr;  // not owned

  std::unique_ptr<PaletteTray::TestApi> test_api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PaletteTrayTest);
};

// Verify the palette tray button exists and is visible with the flags we added
// during setup.
TEST_F(PaletteTrayTest, PaletteTrayIsVisible) {
  ASSERT_TRUE(palette_tray_);
  EXPECT_TRUE(palette_tray_->visible());
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

}  // namespace ash
