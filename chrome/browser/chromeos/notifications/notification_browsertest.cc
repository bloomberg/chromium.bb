// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/notifications/balloon_collection_impl.h"
#include "chrome/browser/chromeos/notifications/notification_panel.h"
#include "chrome/browser/chromeos/notifications/system_notification_factory.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"

namespace chromeos {

class SystemNotificationDelegate : public NotificationDelegate {
 public:
  explicit SystemNotificationDelegate(const std::string id) : id_(id) {}

  virtual void Display() {}
  virtual void Error() {}
  virtual void Close(bool by_user) {}
  virtual std::string id() const { return id_; }

 private:
  std::string id_;

  DISALLOW_COPY_AND_ASSIGN(SystemNotificationDelegate);
};

class NotificationTest : public InProcessBrowserTest {
 public:
  NotificationTest() {
  }

 protected:
  BalloonCollectionImpl* GetBalloonCollectionImpl() {
    return static_cast<BalloonCollectionImpl*>(
        g_browser_process->notification_ui_manager()->balloon_collection());
  }

  NotificationPanel* GetNotificationPanel() {
    return static_cast<NotificationPanel*>(
        GetBalloonCollectionImpl()->notification_ui());
  }
};

IN_PROC_BROWSER_TEST_F(NotificationTest, TestSystemNotification) {
  BalloonCollectionImpl* collection = GetBalloonCollectionImpl();
  NotificationPanel* panel = GetNotificationPanel();
  scoped_refptr<SystemNotificationDelegate> delegate(
      new SystemNotificationDelegate("power"));

  Notification notify = SystemNotificationFactory::Create(
      GURL(), ASCIIToUTF16("Title"), ASCIIToUTF16("test"), delegate.get());
  collection->AddSystemNotification(notify, browser()->profile(), true, false);

  EXPECT_EQ(1, panel->GetNewNotificationCount());
  EXPECT_EQ(1, panel->GetStickyNotificationCount());

  Notification update = SystemNotificationFactory::Create(
      GURL(), ASCIIToUTF16("Title"), ASCIIToUTF16("updated"), delegate.get());
  collection->UpdateNotification(update);

  EXPECT_EQ(1, panel->GetStickyNotificationCount());

  // Dismiss the notification.
  // TODO(oshima): Consider updating API to Remove(NotificationDelegate)
  // or Remove(std::string id);
  collection->Remove(Notification(GURL(), GURL(),
                                  std::wstring(), delegate.get()));

  MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  ui_test_utils::RunMessageLoop();

  EXPECT_EQ(0, panel->GetStickyNotificationCount());
  EXPECT_EQ(0, panel->GetNewNotificationCount());
  // TODO(oshima): check content, etc..
}
}  // namespace chromeos
