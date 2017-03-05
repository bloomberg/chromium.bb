// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/mru_window_tracker.h"

#include "ash/common/test/ash_test.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ui/base/hit_test.h"

namespace ash {

class MruWindowTrackerTest : public AshTest {
 public:
  MruWindowTrackerTest() {}
  ~MruWindowTrackerTest() override {}

  std::unique_ptr<WindowOwner> CreateTestWindow() {
    return AshTest::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  }

  MruWindowTracker* mru_window_tracker() {
    return WmShell::Get()->mru_window_tracker();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MruWindowTrackerTest);
};

// Basic test that the activation order is tracked.
TEST_F(MruWindowTrackerTest, Basic) {
  std::unique_ptr<WindowOwner> w1_owner(CreateTestWindow());
  WmWindow* w1 = w1_owner->window();
  std::unique_ptr<WindowOwner> w2_owner(CreateTestWindow());
  WmWindow* w2 = w2_owner->window();
  std::unique_ptr<WindowOwner> w3_owner(CreateTestWindow());
  WmWindow* w3 = w3_owner->window();
  w3->Activate();
  w2->Activate();
  w1->Activate();

  WmWindow::Windows window_list = mru_window_tracker()->BuildMruWindowList();
  ASSERT_EQ(3u, window_list.size());
  EXPECT_EQ(w1, window_list[0]);
  EXPECT_EQ(w2, window_list[1]);
  EXPECT_EQ(w3, window_list[2]);
}

// Test that minimized windows are not treated specially.
TEST_F(MruWindowTrackerTest, MinimizedWindowsAreLru) {
  // TODO(sky): fix me. Fails in mash because of http://crbug.com/654887.
  if (WmShell::Get()->IsRunningInMash())
    return;

  std::unique_ptr<WindowOwner> w1_owner(CreateTestWindow());
  WmWindow* w1 = w1_owner->window();
  std::unique_ptr<WindowOwner> w2_owner(CreateTestWindow());
  WmWindow* w2 = w2_owner->window();
  std::unique_ptr<WindowOwner> w3_owner(CreateTestWindow());
  WmWindow* w3 = w3_owner->window();
  std::unique_ptr<WindowOwner> w4_owner(CreateTestWindow());
  WmWindow* w4 = w4_owner->window();
  std::unique_ptr<WindowOwner> w5_owner(CreateTestWindow());
  WmWindow* w5 = w5_owner->window();
  std::unique_ptr<WindowOwner> w6_owner(CreateTestWindow());
  WmWindow* w6 = w6_owner->window();
  w6->Activate();
  w5->Activate();
  w4->Activate();
  w3->Activate();
  w2->Activate();
  w1->Activate();

  w1->GetWindowState()->Minimize();
  w4->GetWindowState()->Minimize();
  w5->GetWindowState()->Minimize();

  // By minimizing the first window, we activate w2 which will move it to the
  // front of the MRU queue.
  EXPECT_TRUE(w2->IsActive());

  WmWindow::Windows window_list = mru_window_tracker()->BuildMruWindowList();
  EXPECT_EQ(w2, window_list[0]);
  EXPECT_EQ(w1, window_list[1]);
  EXPECT_EQ(w3, window_list[2]);
  EXPECT_EQ(w4, window_list[3]);
  EXPECT_EQ(w5, window_list[4]);
  EXPECT_EQ(w6, window_list[5]);
}

// Tests that windows being dragged are only in the WindowList once.
TEST_F(MruWindowTrackerTest, DraggedWindowsInListOnlyOnce) {
  std::unique_ptr<WindowOwner> w1_owner(CreateTestWindow());
  WmWindow* w1 = w1_owner->window();
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
