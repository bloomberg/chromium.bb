// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/ash_popup_alignment_delegate.h"

#include <utility>
#include <vector>

#include "ash/display/display_manager.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/message_center/message_center_style.h"

namespace ash {

class AshPopupAlignmentDelegateTest : public test::AshTestBase {
 public:
  AshPopupAlignmentDelegateTest() {}
  ~AshPopupAlignmentDelegateTest() override {}

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    test::AshTestBase::SetUp();
    SetAlignmentDelegate(base::WrapUnique(new AshPopupAlignmentDelegate(
        Shelf::ForPrimaryDisplay()->shelf_layout_manager())));
  }

  void TearDown() override {
    alignment_delegate_.reset();
    test::AshTestBase::TearDown();
  }

  void SetKeyboardBounds(const gfx::Rect& new_bounds) {
    Shelf::ForPrimaryDisplay()
        ->shelf_layout_manager()
        ->OnKeyboardBoundsChanging(new_bounds);
  }

 protected:
  enum Position {
    TOP_LEFT,
    TOP_RIGHT,
    BOTTOM_LEFT,
    BOTTOM_RIGHT,
    OUTSIDE
  };

  AshPopupAlignmentDelegate* alignment_delegate() {
    return alignment_delegate_.get();
  }

  void UpdateWorkArea(AshPopupAlignmentDelegate* alignment_delegate,
                      const display::Display& display) {
    alignment_delegate->StartObserving(display::Screen::GetScreen(), display);
    // Update the layout
    alignment_delegate->OnDisplayWorkAreaInsetsChanged();
  }

  void SetAlignmentDelegate(
      std::unique_ptr<AshPopupAlignmentDelegate> delegate) {
    if (!delegate.get()) {
      alignment_delegate_.reset();
      return;
    }
    alignment_delegate_ = std::move(delegate);
    UpdateWorkArea(alignment_delegate_.get(),
                   display::Screen::GetScreen()->GetPrimaryDisplay());
  }

  Position GetPositionInDisplay(const gfx::Point& point) {
    const gfx::Rect& work_area =
        display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
    const gfx::Point center_point = work_area.CenterPoint();
    if (work_area.x() > point.x() || work_area.y() > point.y() ||
        work_area.right() < point.x() || work_area.bottom() < point.y()) {
      return OUTSIDE;
    }

    if (center_point.x() < point.x())
      return (center_point.y() < point.y()) ? BOTTOM_RIGHT : TOP_RIGHT;
    else
      return (center_point.y() < point.y()) ? BOTTOM_LEFT : TOP_LEFT;
  }

  gfx::Rect GetWorkArea() {
    return alignment_delegate_->work_area_;
  }

 private:
  std::unique_ptr<AshPopupAlignmentDelegate> alignment_delegate_;
};

#if defined(OS_WIN) && !defined(USE_ASH)
// TODO(msw): Broken on Windows. http://crbug.com/584038
#define MAYBE_ShelfAlignment DISABLED_ShelfAlignment
#else
#define MAYBE_ShelfAlignment ShelfAlignment
#endif
TEST_F(AshPopupAlignmentDelegateTest, MAYBE_ShelfAlignment) {
  const gfx::Rect toast_size(0, 0, 10, 10);
  UpdateDisplay("600x600");
  gfx::Point toast_point;
  toast_point.set_x(alignment_delegate()->GetToastOriginX(toast_size));
  toast_point.set_y(alignment_delegate()->GetBaseLine());
  EXPECT_EQ(BOTTOM_RIGHT, GetPositionInDisplay(toast_point));
  EXPECT_FALSE(alignment_delegate()->IsTopDown());
  EXPECT_FALSE(alignment_delegate()->IsFromLeft());

  Shell::GetInstance()->SetShelfAlignment(wm::SHELF_ALIGNMENT_RIGHT,
                                          Shell::GetPrimaryRootWindow());
  toast_point.set_x(alignment_delegate()->GetToastOriginX(toast_size));
  toast_point.set_y(alignment_delegate()->GetBaseLine());
  EXPECT_EQ(BOTTOM_RIGHT, GetPositionInDisplay(toast_point));
  EXPECT_FALSE(alignment_delegate()->IsTopDown());
  EXPECT_FALSE(alignment_delegate()->IsFromLeft());

  Shell::GetInstance()->SetShelfAlignment(wm::SHELF_ALIGNMENT_LEFT,
                                          Shell::GetPrimaryRootWindow());
  toast_point.set_x(alignment_delegate()->GetToastOriginX(toast_size));
  toast_point.set_y(alignment_delegate()->GetBaseLine());
  EXPECT_EQ(BOTTOM_LEFT, GetPositionInDisplay(toast_point));
  EXPECT_FALSE(alignment_delegate()->IsTopDown());
  EXPECT_TRUE(alignment_delegate()->IsFromLeft());
}

TEST_F(AshPopupAlignmentDelegateTest, LockScreen) {
  if (!SupportsHostWindowResize())
    return;

  const gfx::Rect toast_size(0, 0, 10, 10);

  Shell::GetInstance()->SetShelfAlignment(wm::SHELF_ALIGNMENT_LEFT,
                                          Shell::GetPrimaryRootWindow());
  gfx::Point toast_point;
  toast_point.set_x(alignment_delegate()->GetToastOriginX(toast_size));
  toast_point.set_y(alignment_delegate()->GetBaseLine());
  EXPECT_EQ(BOTTOM_LEFT, GetPositionInDisplay(toast_point));
  EXPECT_FALSE(alignment_delegate()->IsTopDown());
  EXPECT_TRUE(alignment_delegate()->IsFromLeft());

  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  toast_point.set_x(alignment_delegate()->GetToastOriginX(toast_size));
  toast_point.set_y(alignment_delegate()->GetBaseLine());
  EXPECT_EQ(BOTTOM_RIGHT, GetPositionInDisplay(toast_point));
  EXPECT_FALSE(alignment_delegate()->IsTopDown());
  EXPECT_FALSE(alignment_delegate()->IsFromLeft());
}

TEST_F(AshPopupAlignmentDelegateTest, AutoHide) {
  const gfx::Rect toast_size(0, 0, 10, 10);
  UpdateDisplay("600x600");
  int origin_x = alignment_delegate()->GetToastOriginX(toast_size);
  int baseline = alignment_delegate()->GetBaseLine();

  // Create a window, otherwise autohide doesn't work.
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  Shell::GetInstance()->SetShelfAutoHideBehavior(
      SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS,
      Shell::GetPrimaryRootWindow());
  Shelf::ForPrimaryDisplay()->shelf_layout_manager()->UpdateAutoHideStateNow();
  EXPECT_EQ(origin_x, alignment_delegate()->GetToastOriginX(toast_size));
  EXPECT_LT(baseline, alignment_delegate()->GetBaseLine());
}

// Verify that docked window doesn't affect the popup alignment.
TEST_F(AshPopupAlignmentDelegateTest, DockedWindow) {
  const gfx::Rect toast_size(0, 0, 10, 10);
  UpdateDisplay("600x600");
  int origin_x = alignment_delegate()->GetToastOriginX(toast_size);
  int baseline = alignment_delegate()->GetBaseLine();

  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(0, 0, 50, 50)));
  aura::Window* docked_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(),
      kShellWindowId_DockedContainer);
  docked_container->AddChild(window.get());

  // Left-side dock should not affect popup alignment
  EXPECT_EQ(origin_x, alignment_delegate()->GetToastOriginX(toast_size));
  EXPECT_EQ(baseline, alignment_delegate()->GetBaseLine());
  EXPECT_FALSE(alignment_delegate()->IsTopDown());
  EXPECT_FALSE(alignment_delegate()->IsFromLeft());

  // Force dock to right-side
  Shell::GetInstance()->SetShelfAlignment(wm::SHELF_ALIGNMENT_LEFT,
                                          Shell::GetPrimaryRootWindow());
  Shell::GetInstance()->SetShelfAlignment(wm::SHELF_ALIGNMENT_BOTTOM,
                                          Shell::GetPrimaryRootWindow());

  // Right-side dock should not affect popup alignment
  EXPECT_EQ(origin_x, alignment_delegate()->GetToastOriginX(toast_size));
  EXPECT_EQ(baseline, alignment_delegate()->GetBaseLine());
  EXPECT_FALSE(alignment_delegate()->IsTopDown());
  EXPECT_FALSE(alignment_delegate()->IsFromLeft());
}

#if defined(OS_WIN) && !defined(USE_ASH)
// TODO(msw): Broken on Windows. http://crbug.com/584038
#define MAYBE_DisplayResize DISABLED_DisplayResize
#else
#define MAYBE_DisplayResize DisplayResize
#endif
TEST_F(AshPopupAlignmentDelegateTest, MAYBE_DisplayResize) {
  const gfx::Rect toast_size(0, 0, 10, 10);
  UpdateDisplay("600x600");
  int origin_x = alignment_delegate()->GetToastOriginX(toast_size);
  int baseline = alignment_delegate()->GetBaseLine();

  UpdateDisplay("800x800");
  EXPECT_LT(origin_x, alignment_delegate()->GetToastOriginX(toast_size));
  EXPECT_LT(baseline, alignment_delegate()->GetBaseLine());

  UpdateDisplay("400x400");
  EXPECT_GT(origin_x, alignment_delegate()->GetToastOriginX(toast_size));
  EXPECT_GT(baseline, alignment_delegate()->GetBaseLine());
}

TEST_F(AshPopupAlignmentDelegateTest, DockedMode) {
  if (!SupportsMultipleDisplays())
    return;

  const gfx::Rect toast_size(0, 0, 10, 10);
  UpdateDisplay("600x600");
  int origin_x = alignment_delegate()->GetToastOriginX(toast_size);
  int baseline = alignment_delegate()->GetBaseLine();

  // Emulate the docked mode; enter to an extended mode, then invoke
  // OnNativeDisplaysChanged() with the info for the secondary display only.
  UpdateDisplay("600x600,800x800");
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();

  std::vector<DisplayInfo> new_info;
  new_info.push_back(
      display_manager->GetDisplayInfo(display_manager->GetDisplayAt(1u).id()));
  display_manager->OnNativeDisplaysChanged(new_info);

  EXPECT_LT(origin_x, alignment_delegate()->GetToastOriginX(toast_size));
  EXPECT_LT(baseline, alignment_delegate()->GetBaseLine());
}

TEST_F(AshPopupAlignmentDelegateTest, TrayHeight) {
  const gfx::Rect toast_size(0, 0, 10, 10);
  UpdateDisplay("600x600");
  int origin_x = alignment_delegate()->GetToastOriginX(toast_size);
  int baseline = alignment_delegate()->GetBaseLine();

  const int kTrayHeight = 100;
  alignment_delegate()->SetSystemTrayHeight(kTrayHeight);

  EXPECT_EQ(origin_x, alignment_delegate()->GetToastOriginX(toast_size));
  EXPECT_EQ(baseline - kTrayHeight - message_center::kMarginBetweenItems,
            alignment_delegate()->GetBaseLine());
}

TEST_F(AshPopupAlignmentDelegateTest, Extended) {
  if (!SupportsMultipleDisplays())
    return;
  UpdateDisplay("600x600,800x800");
  SetAlignmentDelegate(base::WrapUnique(new AshPopupAlignmentDelegate(
      Shelf::ForPrimaryDisplay()->shelf_layout_manager())));

  display::Display second_display = ScreenUtil::GetSecondaryDisplay();
  aura::Window* second_root =
      Shell::GetInstance()
          ->window_tree_host_manager()
          ->GetRootWindowForDisplayId(second_display.id());
  AshPopupAlignmentDelegate for_2nd_display(
      Shelf::ForWindow(second_root)->shelf_layout_manager());
  UpdateWorkArea(&for_2nd_display, second_display);
  // Make sure that the toast position on the secondary display is
  // positioned correctly.
  EXPECT_LT(1300, for_2nd_display.GetToastOriginX(gfx::Rect(0, 0, 10, 10)));
  EXPECT_LT(700, for_2nd_display.GetBaseLine());
}

TEST_F(AshPopupAlignmentDelegateTest, Unified) {
  if (!SupportsMultipleDisplays())
    return;
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  display_manager->SetUnifiedDesktopEnabled(true);

  // Reset the delegate as the primary display's shelf will be destroyed during
  // transition.
  SetAlignmentDelegate(nullptr);

  UpdateDisplay("600x600,800x800");
  SetAlignmentDelegate(base::WrapUnique(new AshPopupAlignmentDelegate(
      Shelf::ForPrimaryDisplay()->shelf_layout_manager())));

  EXPECT_GT(600,
            alignment_delegate()->GetToastOriginX(gfx::Rect(0, 0, 10, 10)));
}

// Tests that when the keyboard is showing that notifications appear above it,
// and that they return to normal once the keyboard is gone.
#if defined(OS_WIN) && !defined(USE_ASH)
// TODO(msw): Broken on Windows. http://crbug.com/584038
#define MAYBE_KeyboardShowing DISABLED_KeyboardShowing
#else
#define MAYBE_KeyboardShowing KeyboardShowing
#endif
TEST_F(AshPopupAlignmentDelegateTest, MAYBE_KeyboardShowing) {
  ASSERT_TRUE(keyboard::IsKeyboardEnabled());
  ASSERT_TRUE(keyboard::IsKeyboardOverscrollEnabled());

  UpdateDisplay("600x600");
  int baseline = alignment_delegate()->GetBaseLine();

  gfx::Rect keyboard_bounds(0, 300, 600, 300);
  SetKeyboardBounds(keyboard_bounds);
  int keyboard_baseline = alignment_delegate()->GetBaseLine();
  EXPECT_NE(baseline, keyboard_baseline);
  EXPECT_GT(keyboard_bounds.y(), keyboard_baseline);

  SetKeyboardBounds(gfx::Rect());
  EXPECT_EQ(baseline, alignment_delegate()->GetBaseLine());
}

}  // namespace ash
