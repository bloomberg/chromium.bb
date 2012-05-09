// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_tray.h"

#include <vector>

#include "ash/system/tray/system_tray_item.h"
#include "ash/test/ash_test_base.h"
#include "ui/views/view.h"

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
    return tray_view_;
  }

  virtual views::View* CreateDefaultView(user::LoginStatus status) {
    default_view_ = new views::View;
    return default_view_;
  }

  virtual views::View* CreateDetailedView(user::LoginStatus status) {
    detailed_view_ = new views::View;
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

  tray->ShowDefaultView();

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
  tray->ShowDefaultView();
  ASSERT_TRUE(test_item->default_view() != NULL);
  ASSERT_TRUE(detailed_item->default_view() != NULL);

  // Show the detailed view, ensure it's created and the default view destroyed.
  tray->ShowDetailedView(detailed_item, 0, false);
  RunAllPendingInMessageLoop();
  ASSERT_TRUE(test_item->default_view() == NULL);
  ASSERT_TRUE(detailed_item->detailed_view() != NULL);

  // Show the default view, ensure it's created and the detailed view destroyed.
  tray->ShowDefaultView();
  RunAllPendingInMessageLoop();
  ASSERT_TRUE(test_item->default_view() != NULL);
  ASSERT_TRUE(detailed_item->detailed_view() == NULL);
}

TEST_F(SystemTrayTest, SystemTrayNotifications) {
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
  tray->ShowDefaultView();
  RunAllPendingInMessageLoop();
  ASSERT_TRUE(test_item->notification_view() == NULL);

  // Show the detailed view, ensure the notificaiton view is created again.
  tray->ShowDetailedView(detailed_item, 0, false);
  RunAllPendingInMessageLoop();
  ASSERT_TRUE(detailed_item->detailed_view() != NULL);
  ASSERT_TRUE(test_item->notification_view() != NULL);

  // Hide the detailed view, ensure the notificaiton view still exists.
  ASSERT_TRUE(tray->CloseBubbleForTest());
  RunAllPendingInMessageLoop();
  ASSERT_TRUE(detailed_item->detailed_view() == NULL);
  ASSERT_TRUE(test_item->notification_view() != NULL);
}

}  // namespace test
}  // namespace ash
