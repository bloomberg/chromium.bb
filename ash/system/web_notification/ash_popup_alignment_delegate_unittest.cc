// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/ash_popup_alignment_delegate.h"

#include <vector>

#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ui/gfx/screen.h"
#include "ui/message_center/message_center_style.h"

namespace ash {

class AshPopupAlignmentDelegateTest : public test::AshTestBase {
 public:
  AshPopupAlignmentDelegateTest() {}
  virtual ~AshPopupAlignmentDelegateTest() {}

  virtual void SetUp() {
    test::AshTestBase::SetUp();
    alignment_delegate_.reset(new AshPopupAlignmentDelegate());
    alignment_delegate_->StartObserving(
        Shell::GetScreen(), Shell::GetScreen()->GetPrimaryDisplay());
  }

  virtual void TearDown() {
    alignment_delegate_.reset();
    test::AshTestBase::TearDown();
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

  Position GetPositionInDisplay(const gfx::Point& point) {
    const gfx::Rect& work_area =
        Shell::GetScreen()->GetPrimaryDisplay().work_area();
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
  scoped_ptr<AshPopupAlignmentDelegate> alignment_delegate_;
};

TEST_F(AshPopupAlignmentDelegateTest, ShelfAlignment) {
  const gfx::Rect toast_size(0, 0, 10, 10);
  UpdateDisplay("600x600");
  gfx::Point toast_point;
  toast_point.set_x(alignment_delegate()->GetToastOriginX(toast_size));
  toast_point.set_y(alignment_delegate()->GetBaseLine());
  EXPECT_EQ(BOTTOM_RIGHT, GetPositionInDisplay(toast_point));
  EXPECT_FALSE(alignment_delegate()->IsTopDown());
  EXPECT_FALSE(alignment_delegate()->IsFromLeft());

  Shell::GetInstance()->SetShelfAlignment(
      SHELF_ALIGNMENT_RIGHT,
      Shell::GetPrimaryRootWindow());
  toast_point.set_x(alignment_delegate()->GetToastOriginX(toast_size));
  toast_point.set_y(alignment_delegate()->GetBaseLine());
  EXPECT_EQ(BOTTOM_RIGHT, GetPositionInDisplay(toast_point));
  EXPECT_FALSE(alignment_delegate()->IsTopDown());
  EXPECT_FALSE(alignment_delegate()->IsFromLeft());

  Shell::GetInstance()->SetShelfAlignment(
      SHELF_ALIGNMENT_LEFT,
      Shell::GetPrimaryRootWindow());
  toast_point.set_x(alignment_delegate()->GetToastOriginX(toast_size));
  toast_point.set_y(alignment_delegate()->GetBaseLine());
  EXPECT_EQ(BOTTOM_LEFT, GetPositionInDisplay(toast_point));
  EXPECT_FALSE(alignment_delegate()->IsTopDown());
  EXPECT_TRUE(alignment_delegate()->IsFromLeft());

  Shell::GetInstance()->SetShelfAlignment(
      SHELF_ALIGNMENT_TOP,
      Shell::GetPrimaryRootWindow());
  toast_point.set_x(alignment_delegate()->GetToastOriginX(toast_size));
  toast_point.set_y(alignment_delegate()->GetBaseLine());
  EXPECT_EQ(TOP_RIGHT, GetPositionInDisplay(toast_point));
  EXPECT_TRUE(alignment_delegate()->IsTopDown());
  EXPECT_FALSE(alignment_delegate()->IsFromLeft());
}

TEST_F(AshPopupAlignmentDelegateTest, LockScreen) {
  const gfx::Rect toast_size(0, 0, 10, 10);

  Shell::GetInstance()->SetShelfAlignment(
      SHELF_ALIGNMENT_LEFT,
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
  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  Shell::GetInstance()->SetShelfAutoHideBehavior(
      SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS,
      Shell::GetPrimaryRootWindow());
  ShelfLayoutManager::ForShelf(Shell::GetPrimaryRootWindow())->
      UpdateAutoHideStateNow();
  EXPECT_EQ(origin_x, alignment_delegate()->GetToastOriginX(toast_size));
  EXPECT_LT(baseline, alignment_delegate()->GetBaseLine());
}

// Verify that docked window doesn't affect the popup alignment.
TEST_F(AshPopupAlignmentDelegateTest, DockedWindow) {
  const gfx::Rect toast_size(0, 0, 10, 10);
  UpdateDisplay("600x600");
  int origin_x = alignment_delegate()->GetToastOriginX(toast_size);
  int baseline = alignment_delegate()->GetBaseLine();

  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(0, 0, 50, 50)));
  aura::Window* docked_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(),
      kShellWindowId_DockedContainer);
  docked_container->AddChild(window.get());

  EXPECT_EQ(origin_x, alignment_delegate()->GetToastOriginX(toast_size));
  EXPECT_EQ(baseline, alignment_delegate()->GetBaseLine());
  EXPECT_FALSE(alignment_delegate()->IsTopDown());
  EXPECT_FALSE(alignment_delegate()->IsFromLeft());
}

TEST_F(AshPopupAlignmentDelegateTest, DisplayResize) {
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

}  // namespace ash
