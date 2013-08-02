// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/web_notification_tray.h"

#include <vector>

#include "ash/display/display_manager.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/test_system_tray_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/message_center_util.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/views/message_center_bubble.h"
#include "ui/message_center/views/message_popup_collection.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

WebNotificationTray* GetTray() {
  return Shell::GetPrimaryRootWindowController()->shelf()->
      status_area_widget()->web_notification_tray();
}

WebNotificationTray* GetSecondaryTray() {
  internal::RootWindowController* primary_controller =
      Shell::GetPrimaryRootWindowController();
  Shell::RootWindowControllerList controllers =
      Shell::GetAllRootWindowControllers();
  for (size_t i = 0; i < controllers.size(); ++i) {
    if (controllers[i] != primary_controller) {
      return controllers[i]->shelf()->
          status_area_widget()->web_notification_tray();
    }
  }

  return NULL;
}

message_center::MessageCenter* GetMessageCenter() {
  return GetTray()->message_center();
}

SystemTray* GetSystemTray() {
  return Shell::GetPrimaryRootWindowController()->shelf()->
      status_area_widget()->system_tray();
}

// Trivial item implementation for testing PopupAndSystemTray test case.
class TestItem : public SystemTrayItem {
 public:
  TestItem() : SystemTrayItem(GetSystemTray()) {}

  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE {
    views::View* default_view = new views::View;
    default_view->SetLayoutManager(new views::FillLayout);
    default_view->AddChildView(new views::Label(UTF8ToUTF16("Default")));
    return default_view;
  }

  virtual views::View* CreateNotificationView(
      user::LoginStatus status) OVERRIDE {
    return new views::View;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestItem);
};

}  // namespace

class WebNotificationTrayTest : public test::AshTestBase {
 public:
  WebNotificationTrayTest() {}
  virtual ~WebNotificationTrayTest() {}

  virtual void TearDown() OVERRIDE {
    GetMessageCenter()->RemoveAllNotifications(false);
    test::AshTestBase::TearDown();
  }

 protected:
  void AddNotification(const std::string& id) {
    scoped_ptr<message_center::Notification> notification;
    notification.reset(new message_center::Notification(
        message_center::NOTIFICATION_TYPE_SIMPLE,
        id,
        ASCIIToUTF16("Test Web Notification"),
        ASCIIToUTF16("Notification message body."),
        gfx::Image(),
        ASCIIToUTF16("www.test.org"),
        "" /* extension id */,
        message_center::RichNotificationData(),
        NULL /* delegate */));
    GetMessageCenter()->AddNotification(notification.Pass());
  }

  void UpdateNotification(const std::string& old_id,
                          const std::string& new_id) {
    scoped_ptr<message_center::Notification> notification;
    notification.reset(new message_center::Notification(
        message_center::NOTIFICATION_TYPE_SIMPLE,
        new_id,
        ASCIIToUTF16("Updated Web Notification"),
        ASCIIToUTF16("Updated message body."),
        gfx::Image(),
        ASCIIToUTF16("www.test.org"),
        "" /* extension id */,
        message_center::RichNotificationData(),
        NULL /* delegate */));
    GetMessageCenter()->UpdateNotification(old_id, notification.Pass());
  }

  void RemoveNotification(const std::string& id) {
    GetMessageCenter()->RemoveNotification(id, false);
  }

  views::Widget* GetWidget() {
    return GetTray()->GetWidget();
  }

  gfx::Rect GetPopupWorkArea() {
    return GetPopupWorkAreaForTray(GetTray());
  }

  gfx::Rect GetPopupWorkAreaForTray(WebNotificationTray* tray) {
    return tray->popup_collection_->work_area_;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebNotificationTrayTest);
};

TEST_F(WebNotificationTrayTest, WebNotifications) {
  // TODO(mukai): move this test case to ui/message_center.
  ASSERT_TRUE(GetWidget());

  // Add a notification.
  AddNotification("test_id1");
  EXPECT_EQ(1u, GetMessageCenter()->NotificationCount());
  EXPECT_TRUE(GetMessageCenter()->HasNotification("test_id1"));
  AddNotification("test_id2");
  AddNotification("test_id2");
  EXPECT_EQ(2u, GetMessageCenter()->NotificationCount());
  EXPECT_TRUE(GetMessageCenter()->HasNotification("test_id2"));

  // Ensure that updating a notification does not affect the count.
  UpdateNotification("test_id2", "test_id3");
  UpdateNotification("test_id3", "test_id3");
  EXPECT_EQ(2u, GetMessageCenter()->NotificationCount());
  EXPECT_FALSE(GetMessageCenter()->HasNotification("test_id2"));

  // Ensure that Removing the first notification removes it from the tray.
  RemoveNotification("test_id1");
  EXPECT_FALSE(GetMessageCenter()->HasNotification("test_id1"));
  EXPECT_EQ(1u, GetMessageCenter()->NotificationCount());

  // Remove the remianing notification.
  RemoveNotification("test_id3");
  EXPECT_EQ(0u, GetMessageCenter()->NotificationCount());
  EXPECT_FALSE(GetMessageCenter()->HasNotification("test_id3"));
}

TEST_F(WebNotificationTrayTest, WebNotificationPopupBubble) {
  // TODO(mukai): move this test case to ui/message_center.
  ASSERT_TRUE(GetWidget());

  // Adding a notification should show the popup bubble.
  AddNotification("test_id1");
  EXPECT_TRUE(GetTray()->IsPopupVisible());

  // Updating a notification should not hide the popup bubble.
  AddNotification("test_id2");
  UpdateNotification("test_id2", "test_id3");
  EXPECT_TRUE(GetTray()->IsPopupVisible());

  // Removing the first notification should not hide the popup bubble.
  RemoveNotification("test_id1");
  EXPECT_TRUE(GetTray()->IsPopupVisible());

  // Removing the visible notification should hide the popup bubble.
  RemoveNotification("test_id3");
  EXPECT_FALSE(GetTray()->IsPopupVisible());
}

using message_center::NotificationList;


// Flakily fails. http://crbug.com/229791
TEST_F(WebNotificationTrayTest, DISABLED_ManyMessageCenterNotifications) {
  // Add the max visible notifications +1, ensure the correct visible number.
  size_t notifications_to_add =
      message_center::kMaxVisibleMessageCenterNotifications + 1;
  for (size_t i = 0; i < notifications_to_add; ++i) {
    std::string id = base::StringPrintf("test_id%d", static_cast<int>(i));
    AddNotification(id);
  }
  bool shown = GetTray()->message_center_tray_->ShowMessageCenterBubble();
  EXPECT_TRUE(shown);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(GetTray()->message_center_bubble() != NULL);
  EXPECT_EQ(notifications_to_add,
            GetMessageCenter()->NotificationCount());
  EXPECT_EQ(message_center::kMaxVisibleMessageCenterNotifications,
            GetTray()->GetMessageCenterBubbleForTest()->
                NumMessageViewsForTest());
}

// Flakily times out. http://crbug.com/229792
TEST_F(WebNotificationTrayTest, DISABLED_ManyPopupNotifications) {
  // Add the max visible popup notifications +1, ensure the correct num visible.
  size_t notifications_to_add =
      message_center::kMaxVisiblePopupNotifications + 1;
  for (size_t i = 0; i < notifications_to_add; ++i) {
    std::string id = base::StringPrintf("test_id%d", static_cast<int>(i));
    AddNotification(id);
  }
  GetTray()->ShowPopups();
  EXPECT_TRUE(GetTray()->IsPopupVisible());
  EXPECT_EQ(notifications_to_add,
            GetMessageCenter()->NotificationCount());
  NotificationList::PopupNotifications popups =
      GetMessageCenter()->GetPopupNotifications();
  EXPECT_EQ(message_center::kMaxVisiblePopupNotifications, popups.size());
}

#if defined(OS_CHROMEOS)
// Display notification is ChromeOS only.
#define MAYBE_PopupShownOnBothDisplays PopupShownOnBothDisplays
#define MAYBE_PopupAndSystemTrayMultiDisplay PopupAndSystemTrayMultiDisplay
#else
#define MAYBE_PopupShownOnBothDisplays DISABLED_PopupShownOnBothDisplays
#define MAYBE_PopupAndSystemTrayMultiDisplay \
  DISABLED_PopupAndSystemTrayMultiDisplay
#endif

// Verifies if the notification appears on both displays when extended mode.
TEST_F(WebNotificationTrayTest, MAYBE_PopupShownOnBothDisplays) {
  if (!SupportsMultipleDisplays())
    return;

  // Enables to appear the notification for display changes.
  test::TestSystemTrayDelegate* tray_delegate =
      static_cast<test::TestSystemTrayDelegate*>(
          Shell::GetInstance()->system_tray_delegate());
  tray_delegate->set_should_show_display_notification(true);

  UpdateDisplay("400x400,200x200");
  // UpdateDisplay() creates the display notifications, so popup is visible.
  EXPECT_TRUE(GetTray()->IsPopupVisible());
  WebNotificationTray* secondary_tray = GetSecondaryTray();
  ASSERT_TRUE(secondary_tray);
  EXPECT_TRUE(secondary_tray->IsPopupVisible());

  // Transition to mirroring and then back to extended display, which recreates
  // root window controller and shelf with having notifications. This code
  // verifies it doesn't cause crash and popups are still visible. See
  // http://crbug.com/263664
  internal::DisplayManager* display_manager =
      Shell::GetInstance()->display_manager();

  display_manager->SetSoftwareMirroring(true);
  UpdateDisplay("400x400,200x200");
  EXPECT_TRUE(GetTray()->IsPopupVisible());
  EXPECT_FALSE(GetSecondaryTray());

  display_manager->SetSoftwareMirroring(false);
  UpdateDisplay("400x400,200x200");
  EXPECT_TRUE(GetTray()->IsPopupVisible());
  secondary_tray = GetSecondaryTray();
  ASSERT_TRUE(secondary_tray);
  EXPECT_TRUE(secondary_tray->IsPopupVisible());
}

#if defined(OS_CHROMEOS)
// PopupAndSystemTray may fail in platforms other than ChromeOS because the
// RootWindow's bound can be bigger than gfx::Display's work area so that
// openingsystem tray doesn't affect at all the work area of popups.
#define MAYBE_PopupAndSystemTray PopupAndSystemTray
#else
#define MAYBE_PopupAndSystemTray DISABLED_PopupAndSystemTray
#endif

TEST_F(WebNotificationTrayTest, MAYBE_PopupAndSystemTray) {
  TestItem* test_item = new TestItem;
  GetSystemTray()->AddTrayItem(test_item);

  AddNotification("test_id");
  EXPECT_TRUE(GetTray()->IsPopupVisible());
  gfx::Rect work_area = GetPopupWorkArea();

  // System tray is created, the popup's work area should be narrowed but still
  // visible.
  GetSystemTray()->ShowDefaultView(BUBBLE_CREATE_NEW);
  EXPECT_TRUE(GetTray()->IsPopupVisible());
  gfx::Rect work_area_with_tray = GetPopupWorkArea();
  EXPECT_GT(work_area.size().GetArea(), work_area_with_tray.size().GetArea());

  // System tray notification is also created, the popup's work area is narrowed
  // even more, but still visible.
  GetSystemTray()->ShowNotificationView(test_item);
  EXPECT_TRUE(GetTray()->IsPopupVisible());
  gfx::Rect work_area_with_tray_notificaiton = GetPopupWorkArea();
  EXPECT_GT(work_area.size().GetArea(),
            work_area_with_tray_notificaiton.size().GetArea());
  EXPECT_GT(work_area_with_tray.size().GetArea(),
            work_area_with_tray_notificaiton.size().GetArea());

  // Close system tray, only system tray notifications.
  GetSystemTray()->ClickedOutsideBubble();
  EXPECT_TRUE(GetTray()->IsPopupVisible());
  gfx::Rect work_area_with_notification = GetPopupWorkArea();
  EXPECT_GT(work_area.size().GetArea(),
            work_area_with_notification.size().GetArea());
  EXPECT_LT(work_area_with_tray_notificaiton.size().GetArea(),
            work_area_with_notification.size().GetArea());

  // Close the notifications.
  GetSystemTray()->HideNotificationView(test_item);
  EXPECT_TRUE(GetTray()->IsPopupVisible());
  EXPECT_EQ(work_area.ToString(), GetPopupWorkArea().ToString());
}

TEST_F(WebNotificationTrayTest, PopupAndSystemTrayAlignment) {
  Shell::GetPrimaryRootWindowController()->GetShelfLayoutManager()->
      SetAlignment(SHELF_ALIGNMENT_LEFT);
  AddNotification("test_id");
  gfx::Rect work_area = GetPopupWorkArea();

  // System tray is created, but the work area is not affected since the tray
  // appears at the left-bottom while the popups appear at the right bottom.
  GetSystemTray()->ShowDefaultView(BUBBLE_CREATE_NEW);
  EXPECT_EQ(work_area.ToString(), GetPopupWorkArea().ToString());
}

TEST_F(WebNotificationTrayTest, MAYBE_PopupAndSystemTrayMultiDisplay) {
  UpdateDisplay("800x600,600x400");

  AddNotification("test_id");
  gfx::Rect work_area = GetPopupWorkArea();
  gfx::Rect work_area_second = GetPopupWorkAreaForTray(GetSecondaryTray());

  // System tray is created on the primary display. The popups in the secondary
  // tray aren't affected.
  GetSystemTray()->ShowDefaultView(BUBBLE_CREATE_NEW);
  EXPECT_GT(work_area.size().GetArea(), GetPopupWorkArea().size().GetArea());
  EXPECT_EQ(work_area_second.ToString(),
            GetPopupWorkAreaForTray(GetSecondaryTray()).ToString());
}

}  // namespace ash
