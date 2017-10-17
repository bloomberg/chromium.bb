// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/message_center/web_notification_tray.h"

#include <stddef.h>

#include <set>

#include "ash/root_window_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray_item.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/message_center_notification_manager.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/message_popup_collection.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace message_center {
namespace {

Notification MakeNotification(const std::string& id) {
  return Notification(NOTIFICATION_TYPE_SIMPLE, id,
                      base::ASCIIToUTF16("Test Web Notification"),
                      base::ASCIIToUTF16("Notification message body."),
                      gfx::Image(), base::ASCIIToUTF16("Some Chrome extension"),
                      GURL("chrome-extension://abbccedd"),
                      NotifierId(NotifierId::APPLICATION, id),
                      RichNotificationData(), new NotificationDelegate());
}

class WebNotificationTrayTest : public InProcessBrowserTest {
 public:
  WebNotificationTrayTest() {}
  ~WebNotificationTrayTest() override {}

  void TearDownOnMainThread() override {
    MessageCenter::Get()->RemoveAllNotifications(
        false, MessageCenter::RemoveType::ALL);
  }

 protected:
  void AddNotification(const std::string& id) {
    g_browser_process->notification_ui_manager()->Add(MakeNotification(id),
                                                      browser()->profile());
  }

  void UpdateNotification(const std::string& id) {
    Notification notification(
        NOTIFICATION_TYPE_SIMPLE, id,
        base::ASCIIToUTF16("Updated Test Web Notification"),
        base::ASCIIToUTF16("Notification message body."), gfx::Image(),
        base::ASCIIToUTF16("Some Chrome extension"),
        GURL("chrome-extension://abbccedd"),
        NotifierId(NotifierId::APPLICATION, id), RichNotificationData(),
        new NotificationDelegate());
    g_browser_process->notification_ui_manager()->Update(notification,
                                                         browser()->profile());
  }

  void RemoveNotification(const std::string& id) {
    g_browser_process->notification_ui_manager()->CancelById(
        id, NotificationUIManager::GetProfileID(browser()->profile()));
  }

  bool HasNotification(const std::string& id) {
    return g_browser_process->notification_ui_manager()->FindById(
               id, browser()->profile()) != nullptr;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebNotificationTrayTest);
};

// TODO(dewittj): More exhaustive testing.
IN_PROC_BROWSER_TEST_F(WebNotificationTrayTest, WebNotifications) {
  MessageCenter* message_center = MessageCenter::Get();

  // Add a notification.
  AddNotification("id1");
  EXPECT_EQ(1u, message_center->NotificationCount());
  EXPECT_TRUE(HasNotification("id1"));
  EXPECT_FALSE(HasNotification("id2"));
  AddNotification("id2");
  AddNotification("id2");
  EXPECT_EQ(2u, message_center->NotificationCount());
  EXPECT_TRUE(HasNotification("id2"));

  // Ensure that updating a notification does not affect the count.
  UpdateNotification("id2");
  EXPECT_EQ(2u, message_center->NotificationCount());

  // Ensure that Removing the first notification removes it from the tray.
  RemoveNotification("id1");
  EXPECT_FALSE(HasNotification("id1"));
  EXPECT_EQ(1u, message_center->NotificationCount());

  // Remove the remaining notification.
  RemoveNotification("id2");
  EXPECT_EQ(0u, message_center->NotificationCount());
  EXPECT_FALSE(HasNotification("id2"));
}

IN_PROC_BROWSER_TEST_F(WebNotificationTrayTest, WebNotificationPopupBubble) {
  std::unique_ptr<WebNotificationTray> tray(new WebNotificationTray());

  // Adding a notification should show the popup bubble.
  AddNotification("id1");
  EXPECT_TRUE(tray->GetMessageCenterTray()->popups_visible());

  // Updating a notification should not hide the popup bubble.
  AddNotification("id2");
  UpdateNotification("id2");
  EXPECT_TRUE(tray->GetMessageCenterTray()->popups_visible());

  // Removing the first notification should not hide the popup bubble.
  RemoveNotification("id1");
  EXPECT_TRUE(tray->GetMessageCenterTray()->popups_visible());

  // Removing the visible notification should hide the popup bubble.
  RemoveNotification("id2");
  EXPECT_FALSE(tray->GetMessageCenterTray()->popups_visible());
}

IN_PROC_BROWSER_TEST_F(WebNotificationTrayTest, ManyPopupNotifications) {
  std::unique_ptr<WebNotificationTray> tray(new WebNotificationTray());

  // Add the max visible popup notifications +1, ensure the correct num visible.
  size_t notifications_to_add = kMaxVisiblePopupNotifications + 1;
  for (size_t i = 0; i < notifications_to_add; ++i) {
    std::string id = base::StringPrintf("id%d", static_cast<int>(i));
    AddNotification(id);
  }
  EXPECT_TRUE(tray->GetMessageCenterTray()->popups_visible());
  MessageCenter* message_center = tray->message_center();
  EXPECT_EQ(notifications_to_add, message_center->NotificationCount());
  NotificationList::PopupNotifications popups =
      message_center->GetPopupNotifications();
  EXPECT_EQ(kMaxVisiblePopupNotifications, popups.size());
}

}  // namespace
}  // namespace message_center
