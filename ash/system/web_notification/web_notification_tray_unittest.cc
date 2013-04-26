// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/web_notification_tray.h"

#include <vector>

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/test/ash_test_base.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "ui/message_center/message_center_constants.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/message_center_util.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/views/message_center_bubble.h"
#include "ui/message_center/views/message_popup_bubble.h"
#include "ui/message_center/views/message_popup_collection.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace ash {

namespace {

WebNotificationTray* GetTray() {
  return Shell::GetPrimaryRootWindowController()->shelf()->
      status_area_widget()->web_notification_tray();
}

message_center::MessageCenter* GetMessageCenter() {
  return GetTray()->message_center();
}

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
    GetMessageCenter()->AddNotification(
        message_center::NOTIFICATION_TYPE_SIMPLE,
        id,
        ASCIIToUTF16("Test Web Notification"),
        ASCIIToUTF16("Notification message body."),
        ASCIIToUTF16("www.test.org"),
        "" /* extension id */,
        NULL /* optional_fields */);
  }

  void UpdateNotification(const std::string& old_id,
                          const std::string& new_id) {
    GetMessageCenter()->UpdateNotification(
        old_id, new_id,
        ASCIIToUTF16("Updated Web Notification"),
        ASCIIToUTF16("Updated message body."),
        NULL);
  }

  void RemoveNotification(const std::string& id) {
    GetMessageCenter()->RemoveNotification(id, false);
  }

  views::Widget* GetWidget() {
    return GetTray()->GetWidget();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebNotificationTrayTest);
};

}  // namespace

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
  // Hide and reshow the bubble so that it is updated immediately, not delayed.
  GetTray()->SetHidePopupBubble(true);
  GetTray()->SetHidePopupBubble(false);
  EXPECT_TRUE(GetTray()->IsPopupVisible());
  EXPECT_EQ(notifications_to_add,
            GetMessageCenter()->NotificationCount());
  if (message_center::IsRichNotificationEnabled()) {
    NotificationList::PopupNotifications popups =
        GetMessageCenter()->GetPopupNotifications();
    EXPECT_EQ(message_center::kMaxVisiblePopupNotifications, popups.size());
  } else {
    EXPECT_EQ(message_center::kMaxVisiblePopupNotifications,
              GetTray()->GetPopupBubbleForTest()->NumMessageViewsForTest());
  }
}

}  // namespace ash
