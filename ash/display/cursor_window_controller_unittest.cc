// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/cursor_window_controller.h"

#include "ash/display/display_util.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/display_manager_test_api.h"
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
    SetCursorCompositionEnabled(true);
  }

  int GetCursorType() const { return cursor_window_controller_->cursor_type_; }

  const gfx::Point& GetCursorHotPoint() const {
    return cursor_window_controller_->hot_point_;
  }

  aura::Window* GetCursorWindow() const {
    return cursor_window_controller_->cursor_window_.get();
  }

  const gfx::ImageSkia& GetCursorImage() const {
    return cursor_window_controller_->GetCursorImageForTest();
  }

  int64_t GetCursorDisplayId() const {
    return cursor_window_controller_->display_.id();
  }

  void SetCursorCompositionEnabled(bool enabled) {
    cursor_window_controller_ = Shell::GetInstance()
                                    ->window_tree_host_manager()
                                    ->cursor_window_controller();
    cursor_window_controller_->SetCursorCompositingEnabled(enabled);
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

  WindowTreeHostManager* window_tree_host_manager =
      Shell::GetInstance()->window_tree_host_manager();
  int64_t primary_display_id = window_tree_host_manager->GetPrimaryDisplayId();
  int64_t secondary_display_id = ScreenUtil::GetSecondaryDisplay().id();
  aura::Window* primary_root =
      window_tree_host_manager->GetRootWindowForDisplayId(primary_display_id);
  aura::Window* secondary_root =
      window_tree_host_manager->GetRootWindowForDisplayId(secondary_display_id);

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
  EXPECT_EQ("3,3", hot_point.ToString());
  cursor_bounds = GetCursorWindow()->GetBoundsInScreen();
  EXPECT_EQ(220, cursor_bounds.x() + hot_point.x());
  EXPECT_EQ(50, cursor_bounds.y() + hot_point.y());
}

// Windows doesn't support compositor based cursor.
#if !defined(OS_WIN)
// Make sure that composition cursor inherits the visibility state.
TEST_F(CursorWindowControllerTest, VisibilityTest) {
  ASSERT_TRUE(GetCursorWindow());
  EXPECT_TRUE(GetCursorWindow()->IsVisible());
  aura::client::CursorClient* client = Shell::GetInstance()->cursor_manager();
  client->HideCursor();
  ASSERT_TRUE(GetCursorWindow());
  EXPECT_FALSE(GetCursorWindow()->IsVisible());

  // Normal cursor should be in the correct state.
  SetCursorCompositionEnabled(false);
  ASSERT_FALSE(GetCursorWindow());
  ASSERT_FALSE(client->IsCursorVisible());

  // Cursor was hidden.
  SetCursorCompositionEnabled(true);
  ASSERT_TRUE(GetCursorWindow());
  EXPECT_FALSE(GetCursorWindow()->IsVisible());

  // Goback to normal cursor and show the cursor.
  SetCursorCompositionEnabled(false);
  ASSERT_FALSE(GetCursorWindow());
  ASSERT_FALSE(client->IsCursorVisible());
  client->ShowCursor();
  ASSERT_TRUE(client->IsCursorVisible());

  // Cursor was shown.
  SetCursorCompositionEnabled(true);
  ASSERT_TRUE(GetCursorWindow());
  EXPECT_TRUE(GetCursorWindow()->IsVisible());
}

// Make sure that composition cursor stays big even when
// the DSF becomes 1x as a result of zooming out.
TEST_F(CursorWindowControllerTest, DSF) {
  UpdateDisplay("1000x500*2");
  int64_t primary_id = gfx::Screen::GetScreen()->GetPrimaryDisplay().id();

  test::ScopedSetInternalDisplayId set_internal(primary_id);
  SetCursorCompositionEnabled(true);
  ASSERT_EQ(
      2.0f,
      gfx::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_TRUE(GetCursorImage().HasRepresentation(2.0f));

  ASSERT_TRUE(SetDisplayUIScale(primary_id, 2.0f));
  ASSERT_EQ(
      1.0f,
      gfx::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_TRUE(GetCursorImage().HasRepresentation(2.0f));
}
#endif

}  // namespace ash
