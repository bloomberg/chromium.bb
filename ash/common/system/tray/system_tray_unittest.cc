// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/system_tray.h"

#include <string>
#include <vector>

#include "ash/common/accelerators/accelerator_controller.h"
#include "ash/common/accessibility_delegate.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/system/status_area_widget.h"
#include "ash/common/system/tray/system_tray_bubble.h"
#include "ash/common/system/tray/system_tray_item.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_item_container.h"
#include "ash/common/system/web_notification/web_notification_tray.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/status_area_widget_test_helper.h"
#include "ash/test/test_system_tray_item.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/separator.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace test {

namespace {

const char kVisibleRowsHistogramName[] =
    "Ash.SystemMenu.DefaultView.VisibleRows";

class ModalWidgetDelegate : public views::WidgetDelegateView {
 public:
  ModalWidgetDelegate() {}
  ~ModalWidgetDelegate() override {}

  ui::ModalType GetModalType() const override { return ui::MODAL_TYPE_SYSTEM; }

 private:
  DISALLOW_COPY_AND_ASSIGN(ModalWidgetDelegate);
};

}  // namespace

typedef AshTestBase SystemTrayTest;

// Verifies only the visible default views are recorded in the
// "Ash.SystemMenu.DefaultView.VisibleItems" histogram.
TEST_F(SystemTrayTest, OnlyVisibleItemsRecorded) {
  SystemTray* tray = GetPrimarySystemTray();
  ASSERT_TRUE(tray->GetWidget());

  TestSystemTrayItem* test_item = new TestSystemTrayItem();
  tray->AddTrayItem(base::WrapUnique(test_item));

  base::HistogramTester histogram_tester;

  tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  RunAllPendingInMessageLoop();
  histogram_tester.ExpectBucketCount(kVisibleRowsHistogramName,
                                     SystemTrayItem::UMA_TEST, 1);

  ASSERT_TRUE(tray->CloseSystemBubble());
  RunAllPendingInMessageLoop();

  tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  RunAllPendingInMessageLoop();
  histogram_tester.ExpectBucketCount(kVisibleRowsHistogramName,
                                     SystemTrayItem::UMA_TEST, 2);

  ASSERT_TRUE(tray->CloseSystemBubble());
  RunAllPendingInMessageLoop();

  test_item->set_views_are_visible(false);

  tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  RunAllPendingInMessageLoop();
  histogram_tester.ExpectBucketCount(kVisibleRowsHistogramName,
                                     SystemTrayItem::UMA_TEST, 2);

  ASSERT_TRUE(tray->CloseSystemBubble());
  RunAllPendingInMessageLoop();
}

// Verifies a visible UMA_NOT_RECORDED default view is not recorded in the
// "Ash.SystemMenu.DefaultView.VisibleItems" histogram.
TEST_F(SystemTrayTest, NotRecordedtemsAreNotRecorded) {
  SystemTray* tray = GetPrimarySystemTray();
  ASSERT_TRUE(tray->GetWidget());

  TestSystemTrayItem* test_item =
      new TestSystemTrayItem(SystemTrayItem::UMA_NOT_RECORDED);
  tray->AddTrayItem(base::WrapUnique(test_item));

  base::HistogramTester histogram_tester;

  tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  RunAllPendingInMessageLoop();
  histogram_tester.ExpectBucketCount(kVisibleRowsHistogramName,
                                     SystemTrayItem::UMA_NOT_RECORDED, 0);

  ASSERT_TRUE(tray->CloseSystemBubble());
  RunAllPendingInMessageLoop();
}

// Verifies null default views are not recorded in the
// "Ash.SystemMenu.DefaultView.VisibleItems" histogram.
TEST_F(SystemTrayTest, NullDefaultViewIsNotRecorded) {
  SystemTray* tray = GetPrimarySystemTray();
  ASSERT_TRUE(tray->GetWidget());

  TestSystemTrayItem* test_item = new TestSystemTrayItem();
  test_item->set_has_views(false);
  tray->AddTrayItem(base::WrapUnique(test_item));

  base::HistogramTester histogram_tester;

  tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  RunAllPendingInMessageLoop();
  histogram_tester.ExpectBucketCount(kVisibleRowsHistogramName,
                                     SystemTrayItem::UMA_TEST, 0);

  ASSERT_TRUE(tray->CloseSystemBubble());
  RunAllPendingInMessageLoop();
}

// Verifies visible detailed views are not recorded in the
// "Ash.SystemMenu.DefaultView.VisibleItems" histogram.
TEST_F(SystemTrayTest, VisibleDetailedViewsIsNotRecorded) {
  SystemTray* tray = GetPrimarySystemTray();
  ASSERT_TRUE(tray->GetWidget());

  TestSystemTrayItem* test_item = new TestSystemTrayItem();
  tray->AddTrayItem(base::WrapUnique(test_item));

  base::HistogramTester histogram_tester;

  tray->ShowDetailedView(test_item, 0, false, BUBBLE_CREATE_NEW);
  RunAllPendingInMessageLoop();

  histogram_tester.ExpectTotalCount(kVisibleRowsHistogramName, 0);

  ASSERT_TRUE(tray->CloseSystemBubble());
  RunAllPendingInMessageLoop();
}

// Verifies visible default views are not recorded for menu re-shows in the
// "Ash.SystemMenu.DefaultView.VisibleItems" histogram.
TEST_F(SystemTrayTest, VisibleDefaultViewIsNotRecordedOnReshow) {
  SystemTray* tray = GetPrimarySystemTray();
  ASSERT_TRUE(tray->GetWidget());

  TestSystemTrayItem* test_item = new TestSystemTrayItem();
  tray->AddTrayItem(base::WrapUnique(test_item));

  base::HistogramTester histogram_tester;

  tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  RunAllPendingInMessageLoop();
  histogram_tester.ExpectBucketCount(kVisibleRowsHistogramName,
                                     SystemTrayItem::UMA_TEST, 1);

  tray->ShowDetailedView(test_item, 0, false, BUBBLE_USE_EXISTING);
  RunAllPendingInMessageLoop();
  histogram_tester.ExpectBucketCount(kVisibleRowsHistogramName,
                                     SystemTrayItem::UMA_TEST, 1);

  tray->ShowDefaultView(BUBBLE_USE_EXISTING);
  RunAllPendingInMessageLoop();
  histogram_tester.ExpectBucketCount(kVisibleRowsHistogramName,
                                     SystemTrayItem::UMA_TEST, 1);

  ASSERT_TRUE(tray->CloseSystemBubble());
  RunAllPendingInMessageLoop();
}

TEST_F(SystemTrayTest, SystemTrayDefaultView) {
  SystemTray* tray = GetPrimarySystemTray();
  ASSERT_TRUE(tray->GetWidget());

  tray->ShowDefaultView(BUBBLE_CREATE_NEW);

  // Ensure that closing the bubble destroys it.
  ASSERT_TRUE(tray->CloseSystemBubble());
  RunAllPendingInMessageLoop();
  ASSERT_FALSE(tray->CloseSystemBubble());
}

// Make sure the opening system tray bubble will not deactivate the
// other window. crbug.com/120680.
TEST_F(SystemTrayTest, Activation) {
  // TODO: investigate why this fails in mash. http://crbug.com/695559.
  if (WmShell::Get()->IsRunningInMash())
    return;

  SystemTray* tray = GetPrimarySystemTray();
  std::unique_ptr<views::Widget> widget(CreateTestWidget(
      nullptr, kShellWindowId_DefaultContainer, gfx::Rect(0, 0, 100, 100)));
  EXPECT_TRUE(widget->IsActive());

  tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  ASSERT_TRUE(tray->GetWidget());
  EXPECT_FALSE(tray->GetSystemBubble()->bubble_view()->GetWidget()->IsActive());
  EXPECT_TRUE(widget->IsActive());

  tray->ActivateBubble();
  EXPECT_TRUE(tray->GetSystemBubble()->bubble_view()->GetWidget()->IsActive());
  EXPECT_FALSE(widget->IsActive());

  // Accelerator will activate the bubble.
  tray->CloseSystemBubble();

  EXPECT_TRUE(widget->IsActive());
  WmShell::Get()->accelerator_controller()->PerformActionIfEnabled(
      SHOW_SYSTEM_TRAY_BUBBLE);
  EXPECT_TRUE(tray->GetSystemBubble()->bubble_view()->GetWidget()->IsActive());
  EXPECT_FALSE(widget->IsActive());
}

// Opening and closing the bubble should change the coloring of the tray.
TEST_F(SystemTrayTest, SystemTrayColoring) {
  SystemTray* tray = GetPrimarySystemTray();
  ASSERT_TRUE(tray->GetWidget());
  // At the beginning the tray coloring is not active.
  ASSERT_FALSE(tray->is_active());

  // Showing the system bubble should show the background as active.
  tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  ASSERT_TRUE(tray->is_active());

  // Closing the system menu should change the coloring back to normal.
  ASSERT_TRUE(tray->CloseSystemBubble());
  RunAllPendingInMessageLoop();
  ASSERT_FALSE(tray->is_active());
}

// Closing the system bubble through an alignment change should change the
// system tray coloring back to normal.
TEST_F(SystemTrayTest, SystemTrayColoringAfterAlignmentChange) {
  SystemTray* tray = GetPrimarySystemTray();
  ASSERT_TRUE(tray->GetWidget());
  WmShelf* shelf = GetPrimaryShelf();
  shelf->SetAlignment(SHELF_ALIGNMENT_BOTTOM);
  // At the beginning the tray coloring is not active.
  ASSERT_FALSE(tray->is_active());

  // Showing the system bubble should show the background as active.
  tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  ASSERT_TRUE(tray->is_active());

  // Changing the alignment should close the system bubble and change the
  // background color.
  shelf->SetAlignment(SHELF_ALIGNMENT_LEFT);
  ASSERT_FALSE(tray->is_active());
  RunAllPendingInMessageLoop();
  // The bubble should already be closed by now.
  ASSERT_FALSE(tray->CloseSystemBubble());
}

TEST_F(SystemTrayTest, SystemTrayTestItems) {
  SystemTray* tray = GetPrimarySystemTray();
  ASSERT_TRUE(tray->GetWidget());

  TestSystemTrayItem* test_item = new TestSystemTrayItem();
  TestSystemTrayItem* detailed_item = new TestSystemTrayItem();
  tray->AddTrayItem(base::WrapUnique(test_item));
  tray->AddTrayItem(base::WrapUnique(detailed_item));

  // Check items have been added.
  std::vector<SystemTrayItem*> items = tray->GetTrayItems();
  ASSERT_TRUE(std::find(items.begin(), items.end(), test_item) != items.end());
  ASSERT_TRUE(std::find(items.begin(), items.end(), detailed_item) !=
              items.end());

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
  SystemTray* tray = GetPrimarySystemTray();
  ASSERT_TRUE(tray->GetWidget());

  // Verify that no crashes occur on items lacking some views.
  TestSystemTrayItem* no_view_item = new TestSystemTrayItem();
  no_view_item->set_has_views(false);
  tray->AddTrayItem(base::WrapUnique(no_view_item));
  tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  tray->ShowDetailedView(no_view_item, 0, false, BUBBLE_USE_EXISTING);
  RunAllPendingInMessageLoop();
}

TEST_F(SystemTrayTest, TrayWidgetAutoResizes) {
  SystemTray* tray = GetPrimarySystemTray();
  ASSERT_TRUE(tray->GetWidget());

  // Add an initial tray item so that the tray gets laid out correctly.
  TestSystemTrayItem* initial_item = new TestSystemTrayItem();
  tray->AddTrayItem(base::WrapUnique(initial_item));

  gfx::Size initial_size = tray->GetWidget()->GetWindowBoundsInScreen().size();

  TestSystemTrayItem* new_item = new TestSystemTrayItem();
  tray->AddTrayItem(base::WrapUnique(new_item));

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

// Test is flaky. http://crbug.com/637978
TEST_F(SystemTrayTest, DISABLED_BubbleCreationTypesTest) {
  SystemTray* tray = GetPrimarySystemTray();
  ASSERT_TRUE(tray->GetWidget());

  TestSystemTrayItem* test_item = new TestSystemTrayItem();
  tray->AddTrayItem(base::WrapUnique(test_item));

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

  EXPECT_EQ(bubble_bounds.ToString(), test_item->detailed_view()
                                          ->GetWidget()
                                          ->GetWindowBoundsInScreen()
                                          .ToString());
  EXPECT_EQ(widget, test_item->detailed_view()->GetWidget());

  tray->ShowDefaultView(BUBBLE_USE_EXISTING);
  RunAllPendingInMessageLoop();

  EXPECT_EQ(bubble_bounds.ToString(), test_item->default_view()
                                          ->GetWidget()
                                          ->GetWindowBoundsInScreen()
                                          .ToString());
  EXPECT_EQ(widget, test_item->default_view()->GetWidget());
}

// Tests that the tray view is laid out properly and is fully contained within
// the shelf widget.
TEST_F(SystemTrayTest, TrayBoundsInWidget) {
  WmShelf* shelf = GetPrimaryShelf();
  StatusAreaWidget* widget = StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  SystemTray* tray = GetPrimarySystemTray();

  // Test in bottom alignment.
  shelf->SetAlignment(SHELF_ALIGNMENT_BOTTOM);
  gfx::Rect window_bounds = widget->GetWindowBoundsInScreen();
  gfx::Rect tray_bounds = tray->GetBoundsInScreen();
  EXPECT_TRUE(window_bounds.Contains(tray_bounds));

  // Test in locked alignment.
  shelf->SetAlignment(SHELF_ALIGNMENT_BOTTOM_LOCKED);
  window_bounds = widget->GetWindowBoundsInScreen();
  tray_bounds = tray->GetBoundsInScreen();
  EXPECT_TRUE(window_bounds.Contains(tray_bounds));

  // Test in the left alignment.
  shelf->SetAlignment(SHELF_ALIGNMENT_LEFT);
  window_bounds = widget->GetWindowBoundsInScreen();
  tray_bounds = tray->GetBoundsInScreen();
  // TODO(estade): Re-enable this check. See crbug.com/660928.
  // EXPECT_TRUE(window_bounds.Contains(tray_bounds));

  // Test in the right alignment.
  shelf->SetAlignment(SHELF_ALIGNMENT_LEFT);
  window_bounds = widget->GetWindowBoundsInScreen();
  tray_bounds = tray->GetBoundsInScreen();
  // TODO(estade): Re-enable this check. See crbug.com/660928.
  // EXPECT_TRUE(window_bounds.Contains(tray_bounds));
}

TEST_F(SystemTrayTest, PersistentBubble) {
  // TODO: investigate why this fails in mash. http://crbug.com/695559.
  if (WmShell::Get()->IsRunningInMash())
    return;

  SystemTray* tray = GetPrimarySystemTray();
  ASSERT_TRUE(tray->GetWidget());

  TestSystemTrayItem* test_item = new TestSystemTrayItem();
  tray->AddTrayItem(base::WrapUnique(test_item));

  std::unique_ptr<views::Widget> widget(CreateTestWidget(
      nullptr, kShellWindowId_DefaultContainer, gfx::Rect(0, 0, 100, 100)));

  // Tests for usual default view while activating a window.
  tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  tray->ActivateBubble();
  ASSERT_TRUE(tray->HasSystemBubble());
  widget->Activate();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(tray->HasSystemBubble());

  tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  ASSERT_TRUE(tray->HasSystemBubble());
  {
    ui::test::EventGenerator& generator = GetEventGenerator();
    generator.set_current_location(gfx::Point(5, 5));
    generator.ClickLeftButton();
    ASSERT_FALSE(tray->HasSystemBubble());
  }

  // Same tests for persistent default view.
  tray->ShowPersistentDefaultView();
  ASSERT_TRUE(tray->HasSystemBubble());
  widget->Activate();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(tray->HasSystemBubble());

  {
    ui::test::EventGenerator& generator = GetEventGenerator();
    generator.set_current_location(gfx::Point(5, 5));
    generator.ClickLeftButton();
    ASSERT_TRUE(tray->HasSystemBubble());
  }

  // Same tests for persistent default view with activation.
  tray->ShowPersistentDefaultView();
  EXPECT_TRUE(tray->HasSystemBubble());
  widget->Activate();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(tray->HasSystemBubble());

  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.set_current_location(gfx::Point(5, 5));
  generator.ClickLeftButton();
  EXPECT_TRUE(tray->HasSystemBubble());
}

TEST_F(SystemTrayTest, WithSystemModal) {
  // Check if the accessibility item is created even with system modal dialog.
  WmShell::Get()->accessibility_delegate()->SetVirtualKeyboardEnabled(true);
  std::unique_ptr<views::Widget> widget(CreateTestWidget(
      new ModalWidgetDelegate, kShellWindowId_SystemModalContainer,
      gfx::Rect(0, 0, 100, 100)));

  SystemTray* tray = GetPrimarySystemTray();
  tray->ShowDefaultView(BUBBLE_CREATE_NEW);

  ASSERT_TRUE(tray->HasSystemBubble());
  const views::View* accessibility =
      tray->GetSystemBubble()->bubble_view()->GetViewByID(
          test::kAccessibilityTrayItemViewId);
  ASSERT_TRUE(accessibility);
  EXPECT_TRUE(accessibility->visible());
  EXPECT_FALSE(tray->GetSystemBubble()->bubble_view()->GetViewByID(
      test::kSettingsTrayItemViewId));

  // Close the modal dialog.
  widget.reset();

  tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  // System modal is gone. The bubble should now contains settings
  // as well.
  accessibility = tray->GetSystemBubble()->bubble_view()->GetViewByID(
      test::kAccessibilityTrayItemViewId);
  ASSERT_TRUE(accessibility);
  EXPECT_TRUE(accessibility->visible());
}

// Tests that if SetVisible(true) is called while animating to hidden that the
// tray becomes visible, and stops animating to hidden.
TEST_F(SystemTrayTest, SetVisibleDuringHideAnimation) {
  SystemTray* tray = GetPrimarySystemTray();
  ASSERT_TRUE(tray->visible());

  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> animation_duration;
  animation_duration.reset(new ui::ScopedAnimationDurationScaleMode(
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

TEST_F(SystemTrayTest, SystemTrayHeightWithBubble) {
  SystemTray* tray = GetPrimarySystemTray();
  WebNotificationTray* notification_tray =
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()
          ->web_notification_tray();

  // Ensure the initial tray bubble height is zero.
  EXPECT_EQ(0, notification_tray->tray_bubble_height_for_test());

  // Show the default view, ensure the tray bubble height is changed.
  tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  RunAllPendingInMessageLoop();
  EXPECT_LT(0, notification_tray->tray_bubble_height_for_test());

  // Hide the default view, ensure the tray bubble height is back to zero.
  ASSERT_TRUE(tray->CloseSystemBubble());
  RunAllPendingInMessageLoop();

  EXPECT_EQ(0, notification_tray->tray_bubble_height_for_test());
}

TEST_F(SystemTrayTest, SeparatorThickness) {
  EXPECT_EQ(kSeparatorWidth, views::Separator::kThickness);
}

}  // namespace test
}  // namespace ash
