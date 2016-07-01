// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/mru_window_tracker.h"

#include "ash/common/shell_window_ids.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_shell.h"
#include "ash/mus/bridge/wm_window_mus.h"
#include "ash/mus/test/wm_test_base.h"
#include "ui/base/hit_test.h"

namespace ash {

class MruWindowTrackerTest : public mus::WmTestBase {
 public:
  MruWindowTrackerTest() {}
  ~MruWindowTrackerTest() override {}

  WmWindow* CreateTestWindow() {
    return mus::WmWindowMus::Get(
        mus::WmTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400)));
  }

  MruWindowTracker* mru_window_tracker() {
    return WmShell::Get()->mru_window_tracker();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MruWindowTrackerTest);
};

// Basic test that the activation order is tracked.
TEST_F(MruWindowTrackerTest, Basic) {
  WmWindow* w1 = CreateTestWindow();
  WmWindow* w2 = CreateTestWindow();
  WmWindow* w3 = CreateTestWindow();
  w3->Activate();
  w2->Activate();
  w1->Activate();

  WmWindow::Windows window_list = mru_window_tracker()->BuildMruWindowList();
  EXPECT_EQ(w1, window_list[0]);
  EXPECT_EQ(w2, window_list[1]);
  EXPECT_EQ(w3, window_list[2]);
}

// Test that minimized windows are considered least recently used (but kept in
// correct relative order).
TEST_F(MruWindowTrackerTest, MinimizedWindowsAreLru) {
  WmWindow* w1 = CreateTestWindow();
  WmWindow* w2 = CreateTestWindow();
  WmWindow* w3 = CreateTestWindow();
  WmWindow* w4 = CreateTestWindow();
  WmWindow* w5 = CreateTestWindow();
  WmWindow* w6 = CreateTestWindow();
  w6->Activate();
  w5->Activate();
  w4->Activate();
  w3->Activate();
  w2->Activate();
  w1->Activate();

  w1->GetWindowState()->Minimize();
  w4->GetWindowState()->Minimize();
  w5->GetWindowState()->Minimize();

  // Expect the relative order of minimized windows to remain the same, but all
  // minimized windows to be at the end of the list.
  WmWindow::Windows window_list = mru_window_tracker()->BuildMruWindowList();
  EXPECT_EQ(w2, window_list[0]);
  EXPECT_EQ(w3, window_list[1]);
  EXPECT_EQ(w6, window_list[2]);
  EXPECT_EQ(w1, window_list[3]);
  EXPECT_EQ(w4, window_list[4]);
  EXPECT_EQ(w5, window_list[5]);
}

// Tests that windows being dragged are only in the WindowList once.
// Disabled, see http://crbug.com/618058.
TEST_F(MruWindowTrackerTest, DISABLED_DraggedWindowsInListOnlyOnce) {
  WmWindow* w1 = CreateTestWindow();
  w1->Activate();

  // Start dragging the window.
  w1->GetWindowState()->CreateDragDetails(
      gfx::Point(), HTRIGHT, aura::client::WINDOW_MOVE_SOURCE_TOUCH);

  // During a drag the window is reparented by the Docked container.
  WmWindow* drag_container = w1->GetRootWindow()->GetChildByShellWindowId(
      kShellWindowId_DockedContainer);
  drag_container->AddChild(w1);
  EXPECT_TRUE(w1->GetWindowState()->is_dragged());

  // The dragged window should only be in the list once.
  WmWindow::Windows window_list =
      mru_window_tracker()->BuildWindowListIgnoreModal();
  EXPECT_EQ(1, std::count(window_list.begin(), window_list.end(), w1));
}

}  // namespace ash
