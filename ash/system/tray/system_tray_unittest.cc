// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_tray.h"

#include <vector>

#include "ash/system/tray/system_tray_item.h"
#include "ash/test/ash_test_base.h"
#include "base/utf_string_conversions.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace test {

namespace {

SystemTray* CreateSystemTray() {
  SystemTray* tray = new SystemTray;
  tray->CreateItems();
  tray->CreateWidget();
  return tray;
}

// Trivial item implementation that tracks its views for testing.
class TestItem : public SystemTrayItem {
 public:
  TestItem() : tray_view_(NULL) {}

  virtual views::View* CreateTrayView(user::LoginStatus status) {
    tray_view_ = new views::View;
    // Add a label so it has non-zero width.
    tray_view_->SetLayoutManager(new views::FillLayout);
    tray_view_->AddChildView(new views::Label(UTF8ToUTF16("Tray")));
    return tray_view_;
  }

  virtual views::View* CreateDefaultView(user::LoginStatus status) {
    default_view_ = new views::View;
    default_view_->SetLayoutManager(new views::FillLayout);
    default_view_->AddChildView(new views::Label(UTF8ToUTF16("Default")));
    return default_view_;
  }

  virtual views::View* CreateDetailedView(user::LoginStatus status) {
    detailed_view_ = new views::View;
    detailed_view_->SetLayoutManager(new views::FillLayout);
    detailed_view_->AddChildView(new views::Label(UTF8ToUTF16("Detailed")));
    return detailed_view_;
  }

  virtual views::View* CreateNotificationView(user::LoginStatus status) {
    notification_view_ = new views::View;
    return notification_view_;
  }

  virtual void DestroyTrayView() {
    tray_view_ = NULL;
  }

  virtual void DestroyDefaultView() {
    default_view_ = NULL;
  }

  virtual void DestroyDetailedView() {
    detailed_view_ = NULL;
  }

  virtual void DestroyNotificationView() {
    notification_view_ = NULL;
  }

  virtual void UpdateAfterLoginStatusChange(user::LoginStatus status) {
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

}  // namespace

typedef AshTestBase SystemTrayTest;

TEST_F(SystemTrayTest, SystemTrayDefaultView) {
  scoped_ptr<SystemTray> tray(CreateSystemTray());
  ASSERT_TRUE(tray->widget());

  tray->ShowDefaultView(BUBBLE_CREATE_NEW);

  // Ensure that closing the bubble destroys it.
  ASSERT_TRUE(tray->CloseBubbleForTest());
  RunAllPendingInMessageLoop();
  ASSERT_FALSE(tray->CloseBubbleForTest());
}

TEST_F(SystemTrayTest, SystemTrayTestItems) {
  scoped_ptr<SystemTray> tray(CreateSystemTray());
  ASSERT_TRUE(tray->widget());

  TestItem* test_item = new TestItem;
  TestItem* detailed_item = new TestItem;
  tray->AddTrayItem(test_item);
  tray->AddTrayItem(detailed_item);

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

TEST_F(SystemTrayTest, TrayWidgetAutoResizes) {
  scoped_ptr<SystemTray> tray(CreateSystemTray());
  ASSERT_TRUE(tray->widget());

  gfx::Size widget_size = tray->widget()->GetWindowScreenBounds().size();

  TestItem* test_item = new TestItem;
  tray->AddTrayItem(test_item);

  gfx::Size new_size = tray->widget()->GetWindowScreenBounds().size();

  // Adding the new item should change the size of the tray.
  EXPECT_NE(widget_size.ToString(), new_size.ToString());

  // Hiding the tray view of the new item should also change the size of the
  // tray.
  test_item->tray_view()->SetVisible(false);
  EXPECT_EQ(widget_size.ToString(),
            tray->widget()->GetWindowScreenBounds().size().ToString());

  test_item->tray_view()->SetVisible(true);
  EXPECT_EQ(new_size.ToString(),
            tray->widget()->GetWindowScreenBounds().size().ToString());
}

// Disabled due to a use-after-free, see http://crbug.com/127539.
TEST_F(SystemTrayTest, DISABLED_SystemTrayNotifications) {
  scoped_ptr<SystemTray> tray(CreateSystemTray());
  ASSERT_TRUE(tray->widget());

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

  // Show the default view, ensure the notification view is destroyed.
  tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  RunAllPendingInMessageLoop();
  ASSERT_TRUE(test_item->notification_view() == NULL);

  // Show the detailed view, ensure the notificaiton view is created again.
  tray->ShowDetailedView(detailed_item, 0, false, BUBBLE_CREATE_NEW);
  RunAllPendingInMessageLoop();
  ASSERT_TRUE(detailed_item->detailed_view() != NULL);
  ASSERT_TRUE(test_item->notification_view() != NULL);

  // Hide the detailed view, ensure the notificaiton view still exists.
  ASSERT_TRUE(tray->CloseBubbleForTest());
  RunAllPendingInMessageLoop();
  ASSERT_TRUE(detailed_item->detailed_view() == NULL);
  ASSERT_TRUE(test_item->notification_view() != NULL);
}

TEST_F(SystemTrayTest, BubbleCreationTypesTest) {
  scoped_ptr<SystemTray> tray(CreateSystemTray());
  ASSERT_TRUE(tray->widget());

  TestItem* test_item = new TestItem;
  tray->AddTrayItem(test_item);

  // Ensure the tray views are created.
  ASSERT_TRUE(test_item->tray_view() != NULL);

  // Show the default view, ensure the notification view is destroyed.
  tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  RunAllPendingInMessageLoop();

  views::Widget* widget = test_item->default_view()->GetWidget();
  gfx::Rect bubble_bounds = widget->GetWindowScreenBounds();

  tray->ShowDetailedView(test_item, 0, true, BUBBLE_USE_EXISTING);
  RunAllPendingInMessageLoop();

  EXPECT_FALSE(test_item->default_view());

  EXPECT_EQ(bubble_bounds.ToString(), test_item->detailed_view()->GetWidget()->
      GetWindowScreenBounds().ToString());
  EXPECT_EQ(widget, test_item->detailed_view()->GetWidget());

  tray->ShowDefaultView(BUBBLE_USE_EXISTING);
  RunAllPendingInMessageLoop();

  EXPECT_EQ(bubble_bounds.ToString(), test_item->default_view()->GetWidget()->
      GetWindowScreenBounds().ToString());
  EXPECT_EQ(widget, test_item->default_view()->GetWidget());
}

}  // namespace test
}  // namespace ash
