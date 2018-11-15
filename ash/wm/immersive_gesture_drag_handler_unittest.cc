// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/immersive_gesture_drag_handler.h"

#include "ash/public/cpp/app_types.h"
#include "ash/shell.h"
#include "ash/system/overview/overview_button_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/window_grid.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/overview/window_selector_item.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "base/time/time.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/events/test/event_generator.h"

namespace ash {

class ImmersiveGestureDragHandlerTest : public AshTestBase {
 public:
  ImmersiveGestureDragHandlerTest() = default;
  ~ImmersiveGestureDragHandlerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    TabletModeControllerTestApi().EnterTabletMode();

    dragged_window_ = CreateTestWindow();
    none_dragged_window_ = CreateTestWindow();
    dragged_window_->SetProperty(aura::client::kAppType,
                                 static_cast<int>(AppType::CHROME_APP));
    handler_ =
        std::make_unique<ImmersiveGestureDragHandler>(dragged_window_.get());
  }

  void TearDown() override {
    handler_.reset();
    none_dragged_window_.reset();
    dragged_window_.reset();
    AshTestBase::TearDown();
  }

 protected:
  // Send gesture event with |type| to |handler_|.
  void SendGestureEvent(const gfx::Point& position,
                        int scroll_x,
                        int scroll_y,
                        ui::EventType type) {
    ui::GestureEvent event = ui::GestureEvent(
        position.x(), position.y(), ui::EF_NONE, base::TimeTicks::Now(),
        ui::GestureEventDetails(type, scroll_x, scroll_y));
    ui::Event::DispatcherApi(&event).set_target(dragged_window_.get());
    handler_->OnGestureEvent(&event);
  }

  std::unique_ptr<aura::Window> dragged_window_;
  std::unique_ptr<aura::Window> none_dragged_window_;
  std::unique_ptr<ImmersiveGestureDragHandler> handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImmersiveGestureDragHandlerTest);
};

// Tests that tap the window in overview grid during window drag should not end
// the overview mode and the window should still be kept in overview grid.
TEST_F(ImmersiveGestureDragHandlerTest,
       TapWindowInOverviewGridDuringWindowDrag) {
  ASSERT_TRUE(TabletModeControllerTestApi().IsTabletModeStarted());

  SendGestureEvent(gfx::Point(0, 0), 0, 5, ui::ET_GESTURE_SCROLL_BEGIN);
  // Drag the window to the right corner to avoid overlap with
  // |none_dragged_window_| in overview grid.
  SendGestureEvent(gfx::Point(700, 500), 700, 500,
                   ui::ET_GESTURE_SCROLL_UPDATE);
  EXPECT_TRUE(wm::GetWindowState(dragged_window_.get())->is_dragged());

  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  WindowSelector* window_selector =
      Shell::Get()->window_selector_controller()->window_selector();
  EXPECT_TRUE(window_selector->IsWindowInOverview(none_dragged_window_.get()));

  WindowGrid* current_grid = window_selector->GetGridWithRootWindow(
      none_dragged_window_->GetRootWindow());
  WindowSelectorItem* item =
      current_grid->GetWindowSelectorItemContaining(none_dragged_window_.get());
  GetEventGenerator()->GestureTapAt(item->GetTransformedBounds().CenterPoint());

  // Overview mode is still active and |none_dragged_window_| is still in
  // overview grid after tried to tap it in overview grid.
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_TRUE(window_selector->IsWindowInOverview(none_dragged_window_.get()));
}

// Tests that tap the overview button during window drag should not end overview
// mode.
TEST_F(ImmersiveGestureDragHandlerTest, TapOverviewButtonDuringWindowDrag) {
  ASSERT_TRUE(TabletModeControllerTestApi().IsTabletModeStarted());

  SendGestureEvent(gfx::Point(0, 0), 0, 5, ui::ET_GESTURE_SCROLL_BEGIN);
  // Drag the window to the right corner to avoid overlap with
  // |none_dragged_window_| in overview grid.
  SendGestureEvent(gfx::Point(700, 500), 700, 500,
                   ui::ET_GESTURE_SCROLL_UPDATE);
  EXPECT_TRUE(wm::GetWindowState(dragged_window_.get())->is_dragged());
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  WindowSelector* window_selector =
      Shell::Get()->window_selector_controller()->window_selector();

  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  OverviewButtonTray* overview_button_tray =
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()->overview_button_tray();
  ASSERT_TRUE(overview_button_tray->visible());
  GetEventGenerator()->GestureTapAt(
      overview_button_tray->GetBoundsInScreen().CenterPoint());

  // Overview mode is still active and |none_dragged_window_| is still in
  // overview grid after tap the overview button.
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_TRUE(window_selector->IsWindowInOverview(none_dragged_window_.get()));
}

}  // namespace ash
