// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_manager.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/system/toast/toast_manager.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace ash {

// Long duration so the timeout doesn't occur.
const int64_t kLongLongDuration = INT64_MAX;

class DummyEvent : public ui::Event {
 public:
  DummyEvent() : Event(ui::ET_UNKNOWN, base::TimeDelta(), 0) {}
  ~DummyEvent() override {}
};

class ToastManagerTest : public test::AshTestBase {
 public:
  ToastManagerTest() {}
  ~ToastManagerTest() override {}

 private:
  void SetUp() override {
    test::AshTestBase::SetUp();

    manager_ = Shell::GetInstance()->toast_manager();

    manager_->ResetToastIdForTesting();
    EXPECT_EQ(0, GetToastId());
  }

 protected:
  ToastManager* manager() { return manager_; }

  int GetToastId() { return manager_->toast_id_for_testing(); }

  ToastOverlay* GetCurrentOverlay() {
    return manager_->GetCurrentOverlayForTesting();
  }

  views::Widget* GetCurrentWidget() {
    ToastOverlay* overlay = GetCurrentOverlay();
    return overlay ? overlay->widget_for_testing() : nullptr;
  }

  std::string GetCurrentText() {
    ToastOverlay* overlay = GetCurrentOverlay();
    return overlay ? overlay->text_ : std::string();
  }

  void ClickDismissButton() {
    ToastOverlay* overlay = GetCurrentOverlay();
    if (overlay)
      overlay->ClickDismissButtonForTesting(DummyEvent());
  }

  void SetShelfAlignment(wm::ShelfAlignment alignment) {
    Shelf::ForPrimaryDisplay()->shelf_layout_manager()->SetAlignment(alignment);
  }

  void SetShelfState(ShelfVisibilityState state) {
    Shelf::ForPrimaryDisplay()->shelf_layout_manager()->SetState(state);
  }

  void SetShelfAutoHideBehavior(ShelfAutoHideBehavior behavior) {
    Shelf::ForPrimaryDisplay()->shelf_layout_manager()->SetAutoHideBehavior(
        behavior);
  }

 private:
  ToastManager* manager_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ToastManagerTest);
};

TEST_F(ToastManagerTest, ShowAndCloseAutomatically) {
  manager()->Show("DUMMY", 10);

  EXPECT_EQ(1, GetToastId());

  while (GetCurrentOverlay() != nullptr)
    base::RunLoop().RunUntilIdle();
}

TEST_F(ToastManagerTest, ShowAndCloseManually) {
  manager()->Show("DUMMY", kLongLongDuration /* prevent timeout */);

  EXPECT_EQ(1, GetToastId());

  EXPECT_FALSE(GetCurrentWidget()->GetLayer()->GetAnimator()->is_animating());

  ClickDismissButton();

  EXPECT_EQ(nullptr, GetCurrentOverlay());
}

TEST_F(ToastManagerTest, ShowAndCloseManuallyDuringAnimation) {
  ui::ScopedAnimationDurationScaleMode slow_animation_duration(
      ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);

  manager()->Show("DUMMY", kLongLongDuration /* prevent timeout */);
  EXPECT_TRUE(GetCurrentWidget()->GetLayer()->GetAnimator()->is_animating());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, GetToastId());
  EXPECT_TRUE(GetCurrentWidget()->GetLayer()->GetAnimator()->is_animating());

  // Close it during animation.
  ClickDismissButton();

  while (GetCurrentOverlay() != nullptr)
    base::RunLoop().RunUntilIdle();
}

TEST_F(ToastManagerTest, QueueMessage) {
  manager()->Show("DUMMY1", 10);
  manager()->Show("DUMMY2", 10);
  manager()->Show("DUMMY3", 10);

  EXPECT_EQ(1, GetToastId());
  EXPECT_EQ("DUMMY1", GetCurrentText());

  while (GetToastId() != 2)
    base::RunLoop().RunUntilIdle();

  EXPECT_EQ("DUMMY2", GetCurrentText());

  while (GetToastId() != 3)
    base::RunLoop().RunUntilIdle();

  EXPECT_EQ("DUMMY3", GetCurrentText());
}

TEST_F(ToastManagerTest, PositionWithVisibleBottomShelf) {
  ShelfLayoutManager* shelf =
      Shelf::ForPrimaryDisplay()->shelf_layout_manager();
  SetShelfState(ash::SHELF_VISIBLE);
  SetShelfAlignment(wm::SHELF_ALIGNMENT_BOTTOM);

  manager()->Show("DUMMY", kLongLongDuration /* prevent timeout */);
  EXPECT_EQ(1, GetToastId());

  gfx::Rect toast_bounds = GetCurrentWidget()->GetWindowBoundsInScreen();
  gfx::Rect root_bounds =
      ScreenUtil::GetShelfDisplayBoundsInRoot(Shell::GetPrimaryRootWindow());

  EXPECT_TRUE(toast_bounds.Intersects(shelf->user_work_area_bounds()));
  EXPECT_NEAR(root_bounds.CenterPoint().x(), toast_bounds.CenterPoint().x(), 1);

  if (SupportsHostWindowResize()) {
    // If host resize is not supported, ShelfLayoutManager::GetIdealBounds()
    // doesn't return correct value.
    gfx::Rect shelf_bounds = shelf->GetIdealBounds();
    EXPECT_FALSE(toast_bounds.Intersects(shelf_bounds));
    EXPECT_EQ(shelf_bounds.y() - 5, toast_bounds.bottom());
    EXPECT_EQ(root_bounds.bottom() - shelf_bounds.height() - 5,
              toast_bounds.bottom());
  }
}

TEST_F(ToastManagerTest, PositionWithAutoHiddenBottomShelf) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(1, 2, 3, 4)));

  ShelfLayoutManager* shelf =
      Shelf::ForPrimaryDisplay()->shelf_layout_manager();
  SetShelfAlignment(wm::SHELF_ALIGNMENT_BOTTOM);
  SetShelfAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  shelf->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  manager()->Show("DUMMY", kLongLongDuration /* prevent timeout */);
  EXPECT_EQ(1, GetToastId());

  gfx::Rect toast_bounds = GetCurrentWidget()->GetWindowBoundsInScreen();
  gfx::Rect root_bounds =
      ScreenUtil::GetShelfDisplayBoundsInRoot(Shell::GetPrimaryRootWindow());

  EXPECT_TRUE(toast_bounds.Intersects(shelf->user_work_area_bounds()));
  EXPECT_NEAR(root_bounds.CenterPoint().x(), toast_bounds.CenterPoint().x(), 1);
  EXPECT_EQ(root_bounds.bottom() - ShelfLayoutManager::kAutoHideSize - 5,
            toast_bounds.bottom());
}

TEST_F(ToastManagerTest, PositionWithHiddenBottomShelf) {
  ShelfLayoutManager* shelf =
      Shelf::ForPrimaryDisplay()->shelf_layout_manager();
  SetShelfAutoHideBehavior(SHELF_AUTO_HIDE_ALWAYS_HIDDEN);
  SetShelfAlignment(wm::SHELF_ALIGNMENT_BOTTOM);
  SetShelfState(ash::SHELF_HIDDEN);

  manager()->Show("DUMMY", kLongLongDuration /* prevent timeout */);
  EXPECT_EQ(1, GetToastId());

  gfx::Rect toast_bounds = GetCurrentWidget()->GetWindowBoundsInScreen();
  gfx::Rect root_bounds =
      ScreenUtil::GetShelfDisplayBoundsInRoot(Shell::GetPrimaryRootWindow());

  EXPECT_TRUE(toast_bounds.Intersects(shelf->user_work_area_bounds()));
  EXPECT_NEAR(root_bounds.CenterPoint().x(), toast_bounds.CenterPoint().x(), 1);
  EXPECT_EQ(root_bounds.bottom() - 5, toast_bounds.bottom());
}

TEST_F(ToastManagerTest, PositionWithVisibleLeftShelf) {
  ShelfLayoutManager* shelf =
      Shelf::ForPrimaryDisplay()->shelf_layout_manager();
  SetShelfState(ash::SHELF_VISIBLE);
  SetShelfAlignment(wm::SHELF_ALIGNMENT_LEFT);

  manager()->Show("DUMMY", kLongLongDuration /* prevent timeout */);
  EXPECT_EQ(1, GetToastId());

  gfx::Rect toast_bounds = GetCurrentWidget()->GetWindowBoundsInScreen();
  gfx::Rect root_bounds =
      ScreenUtil::GetShelfDisplayBoundsInRoot(Shell::GetPrimaryRootWindow());

  EXPECT_TRUE(toast_bounds.Intersects(shelf->user_work_area_bounds()));
  EXPECT_EQ(root_bounds.bottom() - 5, toast_bounds.bottom());

  if (SupportsHostWindowResize()) {
    // If host resize is not supported, ShelfLayoutManager::GetIdealBounds()
    // doesn't return correct value.
    gfx::Rect shelf_bounds = shelf->GetIdealBounds();
    EXPECT_FALSE(toast_bounds.Intersects(shelf_bounds));
    EXPECT_EQ(
        shelf_bounds.right() + (root_bounds.width() - shelf_bounds.width()) / 2,
        toast_bounds.CenterPoint().x());
  }
}

TEST_F(ToastManagerTest, PositionWithUnifiedDesktop) {
  if (!SupportsMultipleDisplays())
    return;

  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  display_manager->SetUnifiedDesktopEnabled(true);
  UpdateDisplay("1000x500,0+600-100x500");

  ShelfLayoutManager* shelf =
      Shelf::ForPrimaryDisplay()->shelf_layout_manager();
  SetShelfState(ash::SHELF_VISIBLE);
  SetShelfAlignment(wm::SHELF_ALIGNMENT_BOTTOM);

  manager()->Show("DUMMY", kLongLongDuration /* prevent timeout */);
  EXPECT_EQ(1, GetToastId());

  gfx::Rect toast_bounds = GetCurrentWidget()->GetWindowBoundsInScreen();
  gfx::Rect root_bounds =
      ScreenUtil::GetShelfDisplayBoundsInRoot(Shell::GetPrimaryRootWindow());

  EXPECT_TRUE(toast_bounds.Intersects(shelf->user_work_area_bounds()));
  EXPECT_TRUE(root_bounds.Contains(toast_bounds));
  EXPECT_NEAR(root_bounds.CenterPoint().x(), toast_bounds.CenterPoint().x(), 1);

  if (SupportsHostWindowResize()) {
    // If host resize is not supported, ShelfLayoutManager::GetIdealBounds()
    // doesn't return correct value.
    gfx::Rect shelf_bounds = shelf->GetIdealBounds();
    EXPECT_FALSE(toast_bounds.Intersects(shelf_bounds));
    EXPECT_EQ(shelf_bounds.y() - 5, toast_bounds.bottom());
    EXPECT_EQ(root_bounds.bottom() - shelf_bounds.height() - 5,
              toast_bounds.bottom());
  }
}

}  // namespace ash
