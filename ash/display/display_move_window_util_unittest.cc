// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_move_window_util.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_util.h"
#include "base/macros.h"
#include "ui/aura/test/test_windows.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/display_layout_builder.h"
#include "ui/display/manager/display_layout_store.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"

namespace ash {

using DisplayMoveWindowUtilTest = AshTestBase;

// Tests that window bounds are not changing after moving to another display. It
// is added as a child window to another root window with the same origin.
TEST_F(DisplayMoveWindowUtilTest, WindowBounds) {
  display::Screen* screen = display::Screen::GetScreen();
  int64_t primary_id = screen->GetPrimaryDisplay().id();
  display::DisplayIdList list =
      display::test::CreateDisplayIdList2(primary_id, primary_id + 1);
  // Layout: [p][1]
  display::DisplayLayoutBuilder builder(primary_id);
  builder.AddDisplayPlacement(list[1], primary_id,
                              display::DisplayPlacement::RIGHT, 0);
  display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
      list, builder.Build());
  UpdateDisplay("400x300,400x300");
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ(gfx::Rect(0, 0, 400, 300),
            display_manager()->GetDisplayAt(0).bounds());
  EXPECT_EQ(gfx::Rect(400, 0, 400, 300),
            display_manager()->GetDisplayAt(1).bounds());

  aura::Window* window =
      CreateTestWindowInShellWithBounds(gfx::Rect(10, 20, 200, 100));
  wm::ActivateWindow(window);
  HandleMoveWindowToDisplay(DisplayMoveWindowDirection::kRight);
  EXPECT_EQ(gfx::Rect(410, 20, 200, 100), window->GetBoundsInScreen());
}

// A horizontal layout for three displays. They are perfectly horizontally
// aligned with no vertical offset.
TEST_F(DisplayMoveWindowUtilTest, ThreeDisplaysHorizontalLayout) {
  display::Screen* screen = display::Screen::GetScreen();
  int64_t primary_id = screen->GetPrimaryDisplay().id();
  display::DisplayIdList list = display::test::CreateDisplayIdListN(
      3, primary_id, primary_id + 1, primary_id + 2);
  // Layout: [p][1][2]
  display::DisplayLayoutBuilder builder(primary_id);
  builder.AddDisplayPlacement(list[1], primary_id,
                              display::DisplayPlacement::RIGHT, 0);
  builder.AddDisplayPlacement(list[2], list[1],
                              display::DisplayPlacement::RIGHT, 0);
  display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
      list, builder.Build());
  UpdateDisplay("400x300,400x300,400x300");
  EXPECT_EQ(3U, display_manager()->GetNumDisplays());
  EXPECT_EQ(gfx::Rect(0, 0, 400, 300),
            display_manager()->GetDisplayAt(0).bounds());
  EXPECT_EQ(gfx::Rect(400, 0, 400, 300),
            display_manager()->GetDisplayAt(1).bounds());
  EXPECT_EQ(gfx::Rect(800, 0, 400, 300),
            display_manager()->GetDisplayAt(2).bounds());

  aura::Window* window =
      CreateTestWindowInShellWithBounds(gfx::Rect(10, 20, 200, 100));
  wm::ActivateWindow(window);
  ASSERT_EQ(list[0], screen->GetDisplayNearestWindow(window).id());

  HandleMoveWindowToDisplay(DisplayMoveWindowDirection::kLeft);
  EXPECT_EQ(list[2], screen->GetDisplayNearestWindow(window).id());
  HandleMoveWindowToDisplay(DisplayMoveWindowDirection::kLeft);
  EXPECT_EQ(list[1], screen->GetDisplayNearestWindow(window).id());

  HandleMoveWindowToDisplay(DisplayMoveWindowDirection::kRight);
  EXPECT_EQ(list[2], screen->GetDisplayNearestWindow(window).id());
  HandleMoveWindowToDisplay(DisplayMoveWindowDirection::kRight);
  EXPECT_EQ(list[0], screen->GetDisplayNearestWindow(window).id());

  // No-op for above/below movement since no display is in the above/below of
  // primary display.
  HandleMoveWindowToDisplay(DisplayMoveWindowDirection::kAbove);
  EXPECT_EQ(list[0], screen->GetDisplayNearestWindow(window).id());
  HandleMoveWindowToDisplay(DisplayMoveWindowDirection::kBelow);
  EXPECT_EQ(list[0], screen->GetDisplayNearestWindow(window).id());
}

// A vertical layout for three displays. They are laid out with horizontal
// offset.
TEST_F(DisplayMoveWindowUtilTest, ThreeDisplaysVerticalLayoutWithOffset) {
  display::Screen* screen = display::Screen::GetScreen();
  int64_t primary_id = screen->GetPrimaryDisplay().id();
  display::DisplayIdList list = display::test::CreateDisplayIdListN(
      3, primary_id, primary_id + 1, primary_id + 2);
  // Layout: [P]
  //          [2]
  //           [1]
  // Note that display [1] is completely in the right of display [p].
  display::DisplayLayoutBuilder builder(primary_id);
  builder.AddDisplayPlacement(list[1], list[2],
                              display::DisplayPlacement::BOTTOM, 200);
  builder.AddDisplayPlacement(list[2], primary_id,
                              display::DisplayPlacement::BOTTOM, 200);
  display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
      list, builder.Build());
  UpdateDisplay("400x300,400x300,400x300");
  EXPECT_EQ(3U, display_manager()->GetNumDisplays());
  EXPECT_EQ(gfx::Rect(0, 0, 400, 300),
            display_manager()->GetDisplayAt(0).bounds());
  EXPECT_EQ(gfx::Rect(200, 300, 400, 300),
            display_manager()->GetDisplayAt(2).bounds());
  EXPECT_EQ(gfx::Rect(400, 600, 400, 300),
            display_manager()->GetDisplayAt(1).bounds());

  aura::Window* window =
      CreateTestWindowInShellWithBounds(gfx::Rect(10, 20, 200, 100));
  wm::ActivateWindow(window);
  ASSERT_EQ(list[0], screen->GetDisplayNearestWindow(window).id());

  HandleMoveWindowToDisplay(DisplayMoveWindowDirection::kAbove);
  EXPECT_EQ(list[1], screen->GetDisplayNearestWindow(window).id());
  HandleMoveWindowToDisplay(DisplayMoveWindowDirection::kAbove);
  EXPECT_EQ(list[2], screen->GetDisplayNearestWindow(window).id());

  HandleMoveWindowToDisplay(DisplayMoveWindowDirection::kBelow);
  EXPECT_EQ(list[1], screen->GetDisplayNearestWindow(window).id());
  HandleMoveWindowToDisplay(DisplayMoveWindowDirection::kBelow);
  EXPECT_EQ(list[0], screen->GetDisplayNearestWindow(window).id());

  // Left/Right is able to be applied between display [p] and [1].
  HandleMoveWindowToDisplay(DisplayMoveWindowDirection::kRight);
  EXPECT_EQ(list[1], screen->GetDisplayNearestWindow(window).id());
  HandleMoveWindowToDisplay(DisplayMoveWindowDirection::kLeft);
  EXPECT_EQ(list[0], screen->GetDisplayNearestWindow(window).id());
}

// A more than three displays layout to verify the select closest strategy.
TEST_F(DisplayMoveWindowUtilTest, SelectClosestCandidate) {
  display::Screen* screen = display::Screen::GetScreen();
  int64_t primary_id = screen->GetPrimaryDisplay().id();
  // Layout:
  // +--------+
  // | 5      |----+
  // +---+----+ 3  |
  //     | 4  |--+----+----+
  //     +----+  | P  | 1  |
  //             +----+----+
  //                  | 2  |
  //                  +----+
  display::DisplayIdList list = display::test::CreateDisplayIdListN(
      6, primary_id, primary_id + 1, primary_id + 2, primary_id + 3,
      primary_id + 4, primary_id + 5);
  display::DisplayLayoutBuilder builder(primary_id);
  builder.AddDisplayPlacement(list[1], primary_id,
                              display::DisplayPlacement::RIGHT, 0);
  builder.AddDisplayPlacement(list[2], list[1],
                              display::DisplayPlacement::BOTTOM, 0);
  builder.AddDisplayPlacement(list[3], primary_id,
                              display::DisplayPlacement::TOP, -200);
  builder.AddDisplayPlacement(list[4], list[3], display::DisplayPlacement::LEFT,
                              200);
  builder.AddDisplayPlacement(list[5], list[3], display::DisplayPlacement::LEFT,
                              -100);
  display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
      list, builder.Build());
  UpdateDisplay("400x300,400x300,400x300,400x300,400x300,800x300");
  EXPECT_EQ(6U, display_manager()->GetNumDisplays());
  EXPECT_EQ(gfx::Rect(0, 0, 400, 300),
            display_manager()->GetDisplayAt(0).bounds());
  EXPECT_EQ(gfx::Rect(400, 0, 400, 300),
            display_manager()->GetDisplayAt(1).bounds());
  EXPECT_EQ(gfx::Rect(400, 300, 400, 300),
            display_manager()->GetDisplayAt(2).bounds());
  EXPECT_EQ(gfx::Rect(-200, -300, 400, 300),
            display_manager()->GetDisplayAt(3).bounds());
  EXPECT_EQ(gfx::Rect(-600, -100, 400, 300),
            display_manager()->GetDisplayAt(4).bounds());
  EXPECT_EQ(gfx::Rect(-1000, -400, 800, 300),
            display_manager()->GetDisplayAt(5).bounds());

  aura::Window* window =
      CreateTestWindowInShellWithBounds(gfx::Rect(10, 20, 200, 100));
  wm::ActivateWindow(window);
  ASSERT_EQ(list[0], screen->GetDisplayNearestWindow(window).id());

  // Moving window to left display, we have [4] and [5] candidate displays in
  // the left direction. They have the same left space but [4] is selected since
  // it is vertically closer to display [p].
  HandleMoveWindowToDisplay(DisplayMoveWindowDirection::kLeft);
  EXPECT_EQ(list[4], screen->GetDisplayNearestWindow(window).id());

  // Moving window to left display, we don't have candidate displays in the left
  // direction. [1][2][3][p] are candidate displays in the cycled direction.
  // [1][2] have the same left space but [1] is selected since it is vertically
  // closer to display [4].
  HandleMoveWindowToDisplay(DisplayMoveWindowDirection::kLeft);
  EXPECT_EQ(list[1], screen->GetDisplayNearestWindow(window).id());

  // Moving window to right display, we don't have candidate displays in the
  // right direction. [p][3][4][5] are candidate displays in the cycled
  // direction. [5] has the furthest right space so it is selected.
  HandleMoveWindowToDisplay(DisplayMoveWindowDirection::kRight);
  EXPECT_EQ(list[5], screen->GetDisplayNearestWindow(window).id());
}

// Tests that a11y alert is sent on handling move window to display.
TEST_F(DisplayMoveWindowUtilTest, A11yAlert) {
  display::Screen* screen = display::Screen::GetScreen();
  int64_t primary_id = screen->GetPrimaryDisplay().id();
  display::DisplayIdList list =
      display::test::CreateDisplayIdList2(primary_id, primary_id + 1);
  // Layout: [p][1]
  display::DisplayLayoutBuilder builder(primary_id);
  builder.AddDisplayPlacement(list[1], primary_id,
                              display::DisplayPlacement::RIGHT, 0);
  display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
      list, builder.Build());
  UpdateDisplay("400x300,400x300");
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ(gfx::Rect(0, 0, 400, 300),
            display_manager()->GetDisplayAt(0).bounds());
  EXPECT_EQ(gfx::Rect(400, 0, 400, 300),
            display_manager()->GetDisplayAt(1).bounds());

  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  TestAccessibilityControllerClient client;
  controller->SetClient(client.CreateInterfacePtrAndBind());

  aura::Window* window =
      CreateTestWindowInShellWithBounds(gfx::Rect(10, 20, 200, 100));
  wm::ActivateWindow(window);
  HandleMoveWindowToDisplay(DisplayMoveWindowDirection::kRight);
  controller->FlushMojoForTest();
  EXPECT_EQ(mojom::AccessibilityAlert::WINDOW_MOVED_TO_RIGHT_DISPLAY,
            client.last_a11y_alert());
}

}  // namespace ash
