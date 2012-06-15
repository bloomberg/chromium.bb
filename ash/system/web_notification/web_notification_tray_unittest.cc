// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/web_notification_tray.h"

#include <vector>

#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/test/ash_test_base.h"
#include "base/utf_string_conversions.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

WebNotificationTray* GetWebNotificationTray() {
  return Shell::GetInstance()->status_area_widget()->web_notification_tray();
}

class TestDelegate : public WebNotificationTray::Delegate {
 public:
  TestDelegate() {}
  virtual ~TestDelegate() {}

  // WebNotificationTray::Delegate overrides.
  virtual void NotificationRemoved(const std::string& notifcation_id) {
    notification_ids_.erase(notifcation_id);
  }

  virtual void DisableExtension(const std::string& notifcation_id) {
  }

  virtual void DisableNotificationsFromSource(
      const std::string& notifcation_id) {
  }

  virtual void ShowSettings(const std::string& notifcation_id) {
  }

  virtual void OnClicked(const std::string& notifcation_id) {
  }

  void AddNotification(WebNotificationTray* tray, const std::string& id) {
    notification_ids_.insert(id);
    tray->AddNotification(id,
                          ASCIIToUTF16("Test Web Notification"),
                          ASCIIToUTF16("Notification message body."),
                          ASCIIToUTF16("www.test.org"),
                          "" /* extension id */);
  }

  void RemoveNotification(WebNotificationTray* tray, const std::string& id) {
    tray->RemoveNotification(id);
  }

  bool HasNotificationId(const std::string& id) {
    return notification_ids_.find(id) != notification_ids_.end();
  }

 private:
  std::set<std::string> notification_ids_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

}  // namespace

typedef test::AshTestBase WebNotificationTrayTest;

TEST_F(WebNotificationTrayTest, WebNotifications) {
  WebNotificationTray* tray = GetWebNotificationTray();
  scoped_ptr<TestDelegate> delegate(new TestDelegate);
  tray->SetDelegate(delegate.get());

  ASSERT_TRUE(tray->GetWidget());

  // Adding a notification should show the bubble.
  delegate->AddNotification(tray, "test_id1");
  EXPECT_TRUE(tray->bubble() != NULL);
  EXPECT_EQ(1, tray->GetNotificationCount());
  delegate->AddNotification(tray, "test_id2");
  delegate->AddNotification(tray, "test_id2");
  EXPECT_EQ(2, tray->GetNotificationCount());
  // Ensure that removing a notification removes it from the tray, and signals
  // the delegate.
  EXPECT_TRUE(delegate->HasNotificationId("test_id2"));
  delegate->RemoveNotification(tray, "test_id2");
  EXPECT_FALSE(delegate->HasNotificationId("test_id2"));
  EXPECT_EQ(1, tray->GetNotificationCount());

  // Removing the last notification should hide the bubble.
  delegate->RemoveNotification(tray, "test_id1");
  EXPECT_EQ(0, tray->GetNotificationCount());
  EXPECT_TRUE(tray->bubble() == NULL);
}

}  // namespace ash
