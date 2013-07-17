// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/message_center/web_notification_tray.h"

#include <set>

#include "ash/root_window_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray_item.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/views/message_center_bubble.h"
#include "ui/message_center/views/message_popup_collection.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace message_center {

namespace {

class WebNotificationTrayTest : public InProcessBrowserTest {
 public:
  WebNotificationTrayTest() {}
  virtual ~WebNotificationTrayTest() {}

  virtual void CleanUpOnMainThread() OVERRIDE {
    message_center::MessageCenter::Get()->RemoveAllNotifications(false);
  }

 protected:
  class TestNotificationDelegate : public ::NotificationDelegate {
   public:
    explicit TestNotificationDelegate(std::string id) : id_(id) {}
    virtual void Display() OVERRIDE {}
    virtual void Error() OVERRIDE {}
    virtual void Close(bool by_user) OVERRIDE {}
    virtual void Click() OVERRIDE {}
    virtual std::string id() const OVERRIDE { return id_; }
    virtual content::RenderViewHost* GetRenderViewHost() const OVERRIDE {
      return NULL;
    }

   private:
    virtual ~TestNotificationDelegate() {}

    std::string id_;
  };

  void AddNotification(const std::string& id, const std::string& replace_id) {
    ::Notification notification(GURL("chrome-extension://abbccedd"),
                                GURL(),
                                ASCIIToUTF16("Test Web Notification"),
                                ASCIIToUTF16("Notification message body."),
                                WebKit::WebTextDirectionDefault,
                                string16(),
                                ASCIIToUTF16(replace_id),
                                new TestNotificationDelegate(id));

    g_browser_process->notification_ui_manager()->Add(
     notification, browser()->profile());
  }

  void UpdateNotification(const std::string& replace_id,
                          const std::string& new_id) {
    ::Notification notification(GURL("chrome-extension://abbccedd"),
                                GURL(""),
                                ASCIIToUTF16("Updated Web Notification"),
                                ASCIIToUTF16("Updated message body."),
                                WebKit::WebTextDirectionDefault,
                                string16(),
                                ASCIIToUTF16(replace_id),
                                new TestNotificationDelegate(new_id));

    g_browser_process->notification_ui_manager()->Add(
     notification, browser()->profile());
  }

  void RemoveNotification(const std::string& id) {
    g_browser_process->notification_ui_manager()->CancelById(id);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebNotificationTrayTest);
};

}  // namespace

// TODO(dewittj): More exhaustive testing.
IN_PROC_BROWSER_TEST_F(WebNotificationTrayTest, WebNotifications) {
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();

  // Add a notification.
  AddNotification("test_id1", "replace_id1");
  EXPECT_EQ(1u, message_center->NotificationCount());
  EXPECT_TRUE(message_center->HasNotification("test_id1"));
  EXPECT_FALSE(message_center->HasNotification("test_id2"));
  AddNotification("test_id2", "replace_id2");
  AddNotification("test_id2", "replace_id2");
  EXPECT_EQ(2u, message_center->NotificationCount());
  EXPECT_TRUE(message_center->HasNotification("test_id2"));

  // Ensure that updating a notification does not affect the count.
  UpdateNotification("replace_id2", "test_id3");
  UpdateNotification("replace_id2", "test_id3");
  EXPECT_EQ(2u, message_center->NotificationCount());
  EXPECT_FALSE(message_center->HasNotification("test_id2"));

  // Ensure that Removing the first notification removes it from the tray.
  RemoveNotification("test_id1");
  EXPECT_FALSE(message_center->HasNotification("test_id1"));
  EXPECT_EQ(1u, message_center->NotificationCount());

  // Remove the remaining notification.
  RemoveNotification("test_id3");
  EXPECT_EQ(0u, message_center->NotificationCount());
  EXPECT_FALSE(message_center->HasNotification("test_id3"));
}

IN_PROC_BROWSER_TEST_F(WebNotificationTrayTest, WebNotificationPopupBubble) {
  scoped_ptr<WebNotificationTray> tray(new WebNotificationTray());
  tray->message_center();

  // Adding a notification should show the popup bubble.
  AddNotification("test_id1", "replace_id1");
  EXPECT_TRUE(tray->message_center_tray_->popups_visible());

  // Updating a notification should not hide the popup bubble.
  AddNotification("test_id2", "replace_id2");
  UpdateNotification("replace_id2", "test_id3");
  EXPECT_TRUE(tray->message_center_tray_->popups_visible());

  // Removing the first notification should not hide the popup bubble.
  RemoveNotification("test_id1");
  EXPECT_TRUE(tray->message_center_tray_->popups_visible());

  // Removing the visible notification should hide the popup bubble.
  RemoveNotification("test_id3");
  EXPECT_FALSE(tray->message_center_tray_->popups_visible());
}

using message_center::NotificationList;

// Flaky, see http://crbug.com/222500 .
IN_PROC_BROWSER_TEST_F(WebNotificationTrayTest,
                       DISABLED_ManyMessageCenterNotifications) {
  scoped_ptr<WebNotificationTray> tray(new WebNotificationTray());
  message_center::MessageCenter* message_center = tray->message_center();

  // Add the max visible notifications +1, ensure the correct visible number.
  size_t notifications_to_add = kMaxVisibleMessageCenterNotifications + 1;
  for (size_t i = 0; i < notifications_to_add; ++i) {
    std::string id = base::StringPrintf("test_id%d", static_cast<int>(i));
    std::string replace_id =
        base::StringPrintf("replace_id%d", static_cast<int>(i));
    AddNotification(id, replace_id);
  }
  bool shown = tray->message_center_tray_->ShowMessageCenterBubble();
  EXPECT_TRUE(shown);
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(tray->message_center_delegate_ != NULL);
  EXPECT_EQ(notifications_to_add, message_center->NotificationCount());
  EXPECT_EQ(kMaxVisibleMessageCenterNotifications,
            tray->message_center_delegate_->NumMessageViewsForTest());
}

IN_PROC_BROWSER_TEST_F(WebNotificationTrayTest, ManyPopupNotifications) {
  scoped_ptr<WebNotificationTray> tray(new WebNotificationTray());
  message_center::MessageCenter* message_center = tray->message_center();

  // Add the max visible popup notifications +1, ensure the correct num visible.
  size_t notifications_to_add = kMaxVisiblePopupNotifications + 1;
  for (size_t i = 0; i < notifications_to_add; ++i) {
    std::string id = base::StringPrintf("test_id%d", static_cast<int>(i));
    std::string replace_id =
        base::StringPrintf("replace_id%d", static_cast<int>(i));
    AddNotification(id, replace_id);
  }
  // Hide and reshow the bubble so that it is updated immediately, not delayed.
  tray->message_center_tray_->HidePopupBubble();
  tray->message_center_tray_->ShowPopupBubble();
  EXPECT_TRUE(tray->message_center_tray_->popups_visible());
  EXPECT_EQ(notifications_to_add, message_center->NotificationCount());
  NotificationList::PopupNotifications popups =
      message_center->GetPopupNotifications();
  EXPECT_EQ(kMaxVisiblePopupNotifications, popups.size());
}

}  // namespace message_center
