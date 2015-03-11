// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/cursor_window_controller.h"

#include "ash/display/display_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/cursor.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/display.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

class CursorWindowControllerTest : public test::AshTestBase {
 public:
  CursorWindowControllerTest() {}
  ~CursorWindowControllerTest() override {}

  // test::AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    cursor_window_controller_ =
        Shell::GetInstance()->display_controller()->cursor_window_controller();
    cursor_window_controller_->SetCursorCompositingEnabled(true);
  }

  int GetCursorType() const { return cursor_window_controller_->cursor_type_; }

  const gfx::Point& GetCursorHotPoint() const {
    return cursor_window_controller_->hot_point_;
  }

  aura::Window* GetCursorWindow() const {
    return cursor_window_controller_->cursor_window_.get();
  }

  int64 GetCursorDisplayId() const {
    return cursor_window_controller_->display_.id();
  }

 private:
  // Not owned.
  CursorWindowController* cursor_window_controller_;

  DISALLOW_COPY_AND_ASSIGN(CursorWindowControllerTest);
};

// Test that the composited cursor moves to another display when the real cursor
// moves to another display.
TEST_F(CursorWindowControllerTest, MoveToDifferentDisplay) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("200x200,200x200*2/r");

  DisplayController* display_controller =
      Shell::GetInstance()->display_controller();
  int64 primary_display_id = display_controller->GetPrimaryDisplayId();
  int64 secondary_display_id = ScreenUtil::GetSecondaryDisplay().id();
  aura::Window* primary_root =
      display_controller->GetRootWindowForDisplayId(primary_display_id);
  aura::Window* secondary_root =
      display_controller->GetRootWindowForDisplayId(secondary_display_id);

  ui::test::EventGenerator primary_generator(primary_root);
  primary_generator.MoveMouseToInHost(20, 50);

  EXPECT_TRUE(primary_root->Contains(GetCursorWindow()));
  EXPECT_EQ(primary_display_id, GetCursorDisplayId());
  EXPECT_EQ(ui::kCursorNull, GetCursorType());
  gfx::Point hot_point = GetCursorHotPoint();
  EXPECT_EQ("4,4", hot_point.ToString());
  gfx::Rect cursor_bounds = GetCursorWindow()->GetBoundsInScreen();
  EXPECT_EQ(20, cursor_bounds.x() + hot_point.x());
  EXPECT_EQ(50, cursor_bounds.y() + hot_point.y());

  // The cursor can only be moved between displays via
  // WindowTreeHost::MoveCursorTo(). EventGenerator uses a hack to move the
  // cursor between displays.
  // Screen location: 220, 50
  // Root location: 20, 50
  secondary_root->MoveCursorTo(gfx::Point(20, 50));

  // Chrome relies on WindowTreeHost::MoveCursorTo() dispatching a mouse move
  // asynchronously. This is implemented in a platform specific way. Generate a
  // fake mouse move instead of waiting.
  gfx::Point new_cursor_position_in_host(20, 50);
  secondary_root->GetHost()->ConvertPointToHost(&new_cursor_position_in_host);
  ui::test::EventGenerator secondary_generator(secondary_root);
  secondary_generator.MoveMouseToInHost(new_cursor_position_in_host);

  EXPECT_TRUE(secondary_root->Contains(GetCursorWindow()));
  EXPECT_EQ(secondary_display_id, GetCursorDisplayId());
  EXPECT_EQ(ui::kCursorNull, GetCursorType());
  hot_point = GetCursorHotPoint();
  EXPECT_EQ("8,9", hot_point.ToString());
  cursor_bounds = GetCursorWindow()->GetBoundsInScreen();
  EXPECT_EQ(220, cursor_bounds.x() + hot_point.x());
  EXPECT_EQ(50, cursor_bounds.y() + hot_point.y());
}

}  // namespace ash
