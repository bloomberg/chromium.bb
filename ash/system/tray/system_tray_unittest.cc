// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_tray.h"

#include <vector>

#include "ash/accessibility_delegate.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace ash {
namespace test {

namespace {

SystemTray* GetSystemTray() {
  return Shell::GetPrimaryRootWindowController()->shelf()->
      status_area_widget()->system_tray();
}

// Trivial item implementation that tracks its views for testing.
class TestItem : public SystemTrayItem {
 public:
  TestItem() : SystemTrayItem(GetSystemTray()), tray_view_(NULL) {}

  virtual views::View* CreateTrayView(user::LoginStatus status) OVERRIDE {
    tray_view_ = new views::View;
    // Add a label so it has non-zero width.
    tray_view_->SetLayoutManager(new views::FillLayout);
    tray_view_->AddChildView(new views::Label(base::UTF8ToUTF16("Tray")));
    return tray_view_;
  }

  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE {
    default_view_ = new views::View;
    default_view_->SetLayoutManager(new views::FillLayout);
    default_view_->AddChildView(new views::Label(base::UTF8ToUTF16("Default")));
    return default_view_;
  }

  virtual views::View* CreateDetailedView(user::LoginStatus status) OVERRIDE {
    detailed_view_ = new views::View;
    detailed_view_->SetLayoutManager(new views::FillLayout);
    detailed_view_->AddChildView(
        new views::Label(base::UTF8ToUTF16("Detailed")));
    return detailed_view_;
  }

  virtual views::View* CreateNotificationView(
      user::LoginStatus status) OVERRIDE {
    notification_view_ = new views::View;
    return notification_view_;
  }

  virtual void DestroyTrayView() OVERRIDE {
    tray_view_ = NULL;
  }

  virtual void DestroyDefaultView() OVERRIDE {
    default_view_ = NULL;
  }

  virtual void DestroyDetailedView() OVERRIDE {
    detailed_view_ = NULL;
  }

  virtual void DestroyNotificationView() OVERRIDE {
    notification_view_ = NULL;
  }

  virtual void UpdateAfterLoginStatusChange(
      user::LoginStatus status) OVERRIDE {
  }

  views::View* tray_view() const { return tray_view_; }
  views::View* default_view() const { return default_view_; }
  views::View* detailed_view() const { return detailed_view_; }
  views::View* notification_view() const { return notification_view_; }

 private:
  views::View* tray_view_;
  views::View* default_view_;
  views::View* detailed_view_;
  views::View* notification_view_;
};

// Trivial item implementation that returns NULL from tray/default/detailed
// view creation methods.
class TestNoViewItem : public SystemTrayItem {
 public:
  TestNoViewItem() : SystemTrayItem(GetSystemTray()) {}

  virtual views::View* CreateTrayView(user::LoginStatus status) OVERRIDE {
    return NULL;
  }

  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE {
    return NULL;
  }

  virtual views::View* CreateDetailedView(user::LoginStatus status) OVERRIDE {
    return NULL;
  }

  virtual views::View* CreateNotificationView(
      user::LoginStatus status) OVERRIDE {
    return NULL;
  }

  virtual void DestroyTrayView() OVERRIDE {}
  virtual void DestroyDefaultView() OVERRIDE {}
  virtual void DestroyDetailedView() OVERRIDE {}
  virtual void DestroyNotificationView() OVERRIDE {}
  virtual void UpdateAfterLoginStatusChange(
      user::LoginStatus status) OVERRIDE {
  }
};

class ModalWidgetDelegate : public views::WidgetDelegateView {
 public:
  ModalWidgetDelegate() {}
  virtual ~ModalWidgetDelegate() {}

  virtual views::View* GetContentsView() OVERRIDE { return this; }
  virtual ui::ModalType GetModalType() const OVERRIDE {
    return ui::MODAL_TYPE_SYSTEM;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ModalWidgetDelegate);
};

}  // namespace

typedef AshTestBase SystemTrayTest;

TEST_F(SystemTrayTest, SystemTrayDefaultView) {
  SystemTray* tray = GetSystemTray();
  ASSERT_TRUE(tray->GetWidget());

  tray->ShowDefaultView(BUBBLE_CREATE_NEW);

  // Ensure that closing the bubble destroys it.
  ASSERT_TRUE(tray->CloseSystemBubble());
  RunAllPendingInMessageLoop();
  ASSERT_FALSE(tray->CloseSystemBubble());
}

// Opening and closing the bubble should change the coloring of the tray.
TEST_F(SystemTrayTest, SystemTrayColoring) {
  SystemTray* tray = GetSystemTray();
  ASSERT_TRUE(tray->GetWidget());
  // At the beginning the tray coloring is not active.
  ASSERT_FALSE(tray->draw_background_as_active());

  // Showing the system bubble should show the background as active.
  tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  ASSERT_TRUE(tray->draw_background_as_active());

  // Closing the system menu should change the coloring back to normal.
  ASSERT_TRUE(tray->CloseSystemBubble());
  RunAllPendingInMessageLoop();
  ASSERT_FALSE(tray->draw_background_as_active());
}

// Closing the system bubble through an alignment change should change the
// system tray coloring back to normal.
TEST_F(SystemTrayTest, SystemTrayColoringAfterAlignmentChange) {
  SystemTray* tray = GetSystemTray();
  ASSERT_TRUE(tray->GetWidget());
  ShelfLayoutManager* manager =
      Shell::GetPrimaryRootWindowController()->shelf()->shelf_layout_manager();
  manager->SetAlignment(SHELF_ALIGNMENT_BOTTOM);
  // At the beginning the tray coloring is not active.
  ASSERT_FALSE(tray->draw_background_as_active());

  // Showing the system bubble should show the background as active.
  tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  ASSERT_TRUE(tray->draw_background_as_active());

  // Changing the alignment should close the system bubble and change the
  // background color.
  manager->SetAlignment(SHELF_ALIGNMENT_LEFT);
  ASSERT_FALSE(tray->draw_background_as_active());
  RunAllPendingInMessageLoop();
  // The bubble should already be closed by now.
  ASSERT_FALSE(tray->CloseSystemBubble());
}

TEST_F(SystemTrayTest, SystemTrayTestItems) {
  SystemTray* tray = GetSystemTray();
  ASSERT_TRUE(tray->GetWidget());

  TestItem* test_item = new TestItem;
  TestItem* detailed_item = new TestItem;
  tray->AddTrayItem(test_item);
  tray->AddTrayItem(detailed_item);

  // Check items have been added
  const std::vector<SystemTrayItem*>& items = tray->GetTrayItems();
  ASSERT_TRUE(
      std::find(items.begin(), items.end(), test_item) != items.end());
  ASSERT_TRUE(
      std::find(items.begin(), items.end(), detailed_item) != items.end());

  // Ensure the tray views are created.
  ASSERT_TRUE(test_item->tray_view() != NULL);
  ASSERT_TRUE(detailed_item->tray_view() != NULL);

  // Ensure a default views are created.
  tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  ASSERT_TRUE(test_item->default_view() != NULL);
  ASSERT_TRUE(detailed_item->default_view() != NULL);

  // Show the detailed view, ensure it's created and the default view destroyed.
  tray->ShowDetailedView(detailed_item, 0, false, BUBBLE_CREATE_NEW);
  RunAllPendingInMessageLoop();
  ASSERT_TRUE(test_item->default_view() == NULL);
  ASSERT_TRUE(detailed_item->detailed_view() != NULL);

  // Show the default view, ensure it's created and the detailed view destroyed.
  tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  RunAllPendingInMessageLoop();
  ASSERT_TRUE(test_item->default_view() != NULL);
  ASSERT_TRUE(detailed_item->detailed_view() == NULL);
}

TEST_F(SystemTrayTest, SystemTrayNoViewItems) {
  SystemTray* tray = GetSystemTray();
  ASSERT_TRUE(tray->GetWidget());

  // Verify that no crashes occur on items lacking some views.
  TestNoViewItem* no_view_item = new TestNoViewItem;
  tray->AddTrayItem(no_view_item);
  tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  tray->ShowDetailedView(no_view_item, 0, false, BUBBLE_USE_EXISTING);
  RunAllPendingInMessageLoop();
}

TEST_F(SystemTrayTest, TrayWidgetAutoResizes) {
  SystemTray* tray = GetSystemTray();
  ASSERT_TRUE(tray->GetWidget());

  // Add an initial tray item so that the tray gets laid out correctly.
  TestItem* initial_item = new TestItem;
  tray->AddTrayItem(initial_item);

  gfx::Size initial_size = tray->GetWidget()->GetWindowBoundsInScreen().size();

  TestItem* new_item = new TestItem;
  tray->AddTrayItem(new_item);

  gfx::Size new_size = tray->GetWidget()->GetWindowBoundsInScreen().size();

  // Adding the new item should change the size of the tray.
  EXPECT_NE(initial_size.ToString(), new_size.ToString());

  // Hiding the tray view of the new item should also change the size of the
  // tray.
  new_item->tray_view()->SetVisible(false);
  EXPECT_EQ(initial_size.ToString(),
            tray->GetWidget()->GetWindowBoundsInScreen().size().ToString());

  new_item->tray_view()->SetVisible(true);
  EXPECT_EQ(new_size.ToString(),
            tray->GetWidget()->GetWindowBoundsInScreen().size().ToString());
}

TEST_F(SystemTrayTest, SystemTrayNotifications) {
  SystemTray* tray = GetSystemTray();
  ASSERT_TRUE(tray->GetWidget());

  TestItem* test_item = new TestItem;
  TestItem* detailed_item = new TestItem;
  tray->AddTrayItem(test_item);
  tray->AddTrayItem(detailed_item);

  // Ensure the tray views are created.
  ASSERT_TRUE(test_item->tray_view() != NULL);
  ASSERT_TRUE(detailed_item->tray_view() != NULL);

  // Ensure a notification view is created.
  tray->ShowNotificationView(test_item);
  ASSERT_TRUE(test_item->notification_view() != NULL);

  // Show the default view, notification view should remain.
  tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  RunAllPendingInMessageLoop();
  ASSERT_TRUE(test_item->notification_view() != NULL);

  // Show the detailed view, ensure the notification view remains.
  tray->ShowDetailedView(detailed_item, 0, false, BUBBLE_CREATE_NEW);
  RunAllPendingInMessageLoop();
  ASSERT_TRUE(detailed_item->detailed_view() != NULL);
  ASSERT_TRUE(test_item->notification_view() != NULL);

  // Hide the detailed view, ensure the notification view still exists.
  ASSERT_TRUE(tray->CloseSystemBubble());
  RunAllPendingInMessageLoop();
  ASSERT_TRUE(detailed_item->detailed_view() == NULL);
  ASSERT_TRUE(test_item->notification_view() != NULL);
}

TEST_F(SystemTrayTest, BubbleCreationTypesTest) {
  SystemTray* tray = GetSystemTray();
  ASSERT_TRUE(tray->GetWidget());

  TestItem* test_item = new TestItem;
  tray->AddTrayItem(test_item);

  // Ensure the tray views are created.
  ASSERT_TRUE(test_item->tray_view() != NULL);

  // Show the default view, ensure the notification view is destroyed.
  tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  RunAllPendingInMessageLoop();

  views::Widget* widget = test_item->default_view()->GetWidget();
  gfx::Rect bubble_bounds = widget->GetWindowBoundsInScreen();

  tray->ShowDetailedView(test_item, 0, true, BUBBLE_USE_EXISTING);
  RunAllPendingInMessageLoop();

  EXPECT_FALSE(test_item->default_view());

  EXPECT_EQ(bubble_bounds.ToString(), test_item->detailed_view()->GetWidget()->
      GetWindowBoundsInScreen().ToString());
  EXPECT_EQ(widget, test_item->detailed_view()->GetWidget());

  tray->ShowDefaultView(BUBBLE_USE_EXISTING);
  RunAllPendingInMessageLoop();

  EXPECT_EQ(bubble_bounds.ToString(), test_item->default_view()->GetWidget()->
      GetWindowBoundsInScreen().ToString());
  EXPECT_EQ(widget, test_item->default_view()->GetWidget());
}

// Tests that the tray is laid out properly and is fully contained within
// the shelf.
TEST_F(SystemTrayTest, TrayBoundsInWidget) {
  ShelfLayoutManager* manager =
      Shell::GetPrimaryRootWindowController()->shelf()->shelf_layout_manager();
  StatusAreaWidget* widget =
      Shell::GetPrimaryRootWindowController()->shelf()->status_area_widget();
  SystemTray* tray = widget->system_tray();

  // Test in bottom alignment.
  manager->SetAlignment(SHELF_ALIGNMENT_BOTTOM);
  gfx::Rect window_bounds = widget->GetWindowBoundsInScreen();
  gfx::Rect tray_bounds = tray->GetBoundsInScreen();
  EXPECT_TRUE(window_bounds.bottom() >= tray_bounds.bottom());
  EXPECT_TRUE(window_bounds.right() >= tray_bounds.right());
  EXPECT_TRUE(window_bounds.x() >= tray_bounds.x());
  EXPECT_TRUE(window_bounds.y() >= tray_bounds.y());

  // Test in the left alignment.
  manager->SetAlignment(SHELF_ALIGNMENT_LEFT);
  window_bounds = widget->GetWindowBoundsInScreen();
  tray_bounds = tray->GetBoundsInScreen();
  EXPECT_TRUE(window_bounds.bottom() >= tray_bounds.bottom());
  EXPECT_TRUE(window_bounds.right() >= tray_bounds.right());
  EXPECT_TRUE(window_bounds.x() >= tray_bounds.x());
  EXPECT_TRUE(window_bounds.y() >= tray_bounds.y());

  // Test in the right alignment.
  manager->SetAlignment(SHELF_ALIGNMENT_LEFT);
  window_bounds = widget->GetWindowBoundsInScreen();
  tray_bounds = tray->GetBoundsInScreen();
  EXPECT_TRUE(window_bounds.bottom() >= tray_bounds.bottom());
  EXPECT_TRUE(window_bounds.right() >= tray_bounds.right());
  EXPECT_TRUE(window_bounds.x() >= tray_bounds.x());
  EXPECT_TRUE(window_bounds.y() >= tray_bounds.y());
}

TEST_F(SystemTrayTest, PersistentBubble) {
  SystemTray* tray = GetSystemTray();
  ASSERT_TRUE(tray->GetWidget());

  TestItem* test_item = new TestItem;
  tray->AddTrayItem(test_item);

  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));

  // Tests for usual default view.
  // Activating window.
  tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  ASSERT_TRUE(tray->HasSystemBubble());
  wm::ActivateWindow(window.get());
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(tray->HasSystemBubble());

  tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  ASSERT_TRUE(tray->HasSystemBubble());
  {
    aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                         gfx::Point(5, 5));
    generator.ClickLeftButton();
    ASSERT_FALSE(tray->HasSystemBubble());
  }

  // Same tests for persistent default view.
  tray->ShowPersistentDefaultView();
  ASSERT_TRUE(tray->HasSystemBubble());
  wm::ActivateWindow(window.get());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(tray->HasSystemBubble());

  {
    aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                         gfx::Point(5, 5));
    generator.ClickLeftButton();
    ASSERT_TRUE(tray->HasSystemBubble());
  }
}

#if defined(OS_CHROMEOS)
// Accessibility/Settings tray items are available only on cros.
#define MAYBE_WithSystemModal WithSystemModal
#else
#define MAYBE_WithSystemModal DISABLED_WithSystemModal
#endif
TEST_F(SystemTrayTest, MAYBE_WithSystemModal) {
  // Check if the accessibility item is created even with system modal
  // dialog.
  Shell::GetInstance()->accessibility_delegate()->SetVirtualKeyboardEnabled(
      true);
  views::Widget* widget = views::Widget::CreateWindowWithContextAndBounds(
      new ModalWidgetDelegate(),
      Shell::GetPrimaryRootWindow(),
      gfx::Rect(0, 0, 100, 100));
  widget->Show();

  SystemTray* tray = GetSystemTray();
  tray->ShowDefaultView(BUBBLE_CREATE_NEW);

  ASSERT_TRUE(tray->HasSystemBubble());
  const views::View* accessibility =
      tray->GetSystemBubble()->bubble_view()->GetViewByID(
          test::kAccessibilityTrayItemViewId);
  ASSERT_TRUE(accessibility);
  EXPECT_TRUE(accessibility->visible());
  EXPECT_FALSE(tray->GetSystemBubble()->bubble_view()->GetViewByID(
      test::kSettingsTrayItemViewId));

  widget->Close();

  tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  // System modal is gone. The bubble should now contains settings
  // as well.
  accessibility = tray->GetSystemBubble()->bubble_view()->GetViewByID(
      test::kAccessibilityTrayItemViewId);
  ASSERT_TRUE(accessibility);
  EXPECT_TRUE(accessibility->visible());

  const views::View* settings =
      tray->GetSystemBubble()->bubble_view()->GetViewByID(
          test::kSettingsTrayItemViewId);
  ASSERT_TRUE(settings);
  EXPECT_TRUE(settings->visible());
}

// Tests that if SetVisible(true) is called while animating to hidden that the
// tray becomes visible, and stops animating to hidden.
TEST_F(SystemTrayTest, SetVisibleDuringHideAnimation) {
  SystemTray* tray = GetSystemTray();
  ASSERT_TRUE(tray->visible());

  scoped_ptr<ui::ScopedAnimationDurationScaleMode> animation_duration;
  animation_duration.reset(
      new ui::ScopedAnimationDurationScaleMode(
          ui::ScopedAnimationDurationScaleMode::SLOW_DURATION));
  tray->SetVisible(false);
  EXPECT_TRUE(tray->visible());
  EXPECT_EQ(0.0f, tray->layer()->GetTargetOpacity());

  tray->SetVisible(true);
  animation_duration.reset();
  tray->layer()->GetAnimator()->StopAnimating();
  EXPECT_TRUE(tray->visible());
  EXPECT_EQ(1.0f, tray->layer()->GetTargetOpacity());
}

}  // namespace test
}  // namespace ash
