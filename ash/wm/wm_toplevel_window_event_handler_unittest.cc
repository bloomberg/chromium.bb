// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/public/cpp/app_types.h"
#include "ash/shell.h"
#include "ash/system/overview/overview_button_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "ash/wm/window_state.h"
#include "base/time/time.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/test/event_generator.h"

namespace ash {

class WmToplevelWindowEventHandlerTest : public AshTestBase {
 public:
  WmToplevelWindowEventHandlerTest() = default;
  ~WmToplevelWindowEventHandlerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    TabletModeControllerTestApi().EnterTabletMode();

    dragged_window_ = CreateTestWindow();
    non_dragged_window_ = CreateTestWindow();
    dragged_window_->SetProperty(aura::client::kAppType,
                                 static_cast<int>(AppType::CHROME_APP));
  }

  void TearDown() override {
    non_dragged_window_.reset();
    dragged_window_.reset();
    AshTestBase::TearDown();
  }

 protected:
  // Send gesture event with |type| to the toplevel window event handler.
  void SendGestureEvent(const gfx::Point& position,
                        int scroll_x,
                        int scroll_y,
                        ui::EventType type) {
    ui::GestureEvent event = ui::GestureEvent(
        position.x(), position.y(), ui::EF_NONE, base::TimeTicks::Now(),
        ui::GestureEventDetails(type, scroll_x, scroll_y));
    ui::Event::DispatcherApi(&event).set_target(dragged_window_.get());
    ash::Shell::Get()->toplevel_window_event_handler()->OnGestureEvent(&event);
  }

  std::unique_ptr<aura::Window> dragged_window_;
  std::unique_ptr<aura::Window> non_dragged_window_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WmToplevelWindowEventHandlerTest);
};

// Tests that tap the window in overview grid during window drag should end
// the overview mode.
TEST_F(WmToplevelWindowEventHandlerTest,
       TapWindowInOverviewGridDuringWindowDrag) {
  ASSERT_TRUE(TabletModeControllerTestApi().IsTabletModeStarted());

  SendGestureEvent(gfx::Point(0, 0), 0, 5, ui::ET_GESTURE_SCROLL_BEGIN);
  // Drag the window to the right corner to avoid overlap with
  // |non_dragged_window_| in overview grid.
  SendGestureEvent(gfx::Point(700, 500), 700, 500,
                   ui::ET_GESTURE_SCROLL_UPDATE);
  EXPECT_TRUE(wm::GetWindowState(dragged_window_.get())->is_dragged());

  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(overview_controller->IsSelecting());
  EXPECT_TRUE(overview_controller->overview_session()->IsWindowInOverview(
      non_dragged_window_.get()));

  OverviewGrid* current_grid =
      overview_controller->overview_session()->GetGridWithRootWindow(
          non_dragged_window_->GetRootWindow());
  OverviewItem* item =
      current_grid->GetOverviewItemContaining(non_dragged_window_.get());
  GetEventGenerator()->GestureTapAt(item->GetTransformedBounds().CenterPoint());

  // Overview mode is no longer active and |non_dragged_window_| is not in the
  // overview grid after tapping it in overview grid.
  EXPECT_FALSE(overview_controller->IsSelecting());
  EXPECT_FALSE(overview_controller->overview_session());
}

// Tests that the window drag will be reverted if the screen is being rotated.
TEST_F(WmToplevelWindowEventHandlerTest, DisplayConfigurationChangeTest) {
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  // Set the screen orientation to LANDSCAPE_PRIMARY.
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kLandscapePrimary);

  ASSERT_TRUE(TabletModeControllerTestApi().IsTabletModeStarted());

  SendGestureEvent(gfx::Point(0, 0), 0, 5, ui::ET_GESTURE_SCROLL_BEGIN);
  // Drag the window to the right corner to avoid overlap with
  // |non_dragged_window_| in overview grid.
  SendGestureEvent(gfx::Point(700, 500), 700, 500,
                   ui::ET_GESTURE_SCROLL_UPDATE);
  EXPECT_TRUE(wm::GetWindowState(dragged_window_.get())->is_dragged());

  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(overview_controller->IsSelecting());
  EXPECT_TRUE(overview_controller->overview_session()->IsWindowInOverview(
      non_dragged_window_.get()));

  // Rotate the screen during drag.
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kPortraitPrimary);
  EXPECT_TRUE(wm::GetWindowState(dragged_window_.get())->IsMaximized());
  EXPECT_FALSE(overview_controller->IsSelecting());
  EXPECT_FALSE(wm::GetWindowState(dragged_window_.get())->is_dragged());
}

}  // namespace ash
