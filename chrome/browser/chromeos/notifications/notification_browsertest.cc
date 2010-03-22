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

class MockNotificationDelegate : public NotificationDelegate {
 public:
  explicit MockNotificationDelegate(const std::string& id) : id_(id) {}

  virtual void Display() {}
  virtual void Error() {}
  virtual void Close(bool by_user) {}
  virtual std::string id() const { return id_; }

 private:
  std::string id_;

  DISALLOW_COPY_AND_ASSIGN(MockNotificationDelegate);
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

  Notification NewMockNotification(const std::string& id) {
    return NewMockNotification(new MockNotificationDelegate(id));
  }

  Notification NewMockNotification(NotificationDelegate* delegate) {
    return SystemNotificationFactory::Create(
        GURL(), ASCIIToUTF16(""), ASCIIToUTF16(""),
        delegate);
  }

  void RunAllPending() {
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
    ui_test_utils::RunMessageLoop();
  }
};

IN_PROC_BROWSER_TEST_F(NotificationTest, TestBasic) {
  BalloonCollectionImpl* collection = GetBalloonCollectionImpl();
  NotificationPanel* panel = GetNotificationPanel();
  NotificationPanelTester* tester = panel->GetTester();

  // Using system notification as regular notification.
  collection->Add(NewMockNotification("1"), browser()->profile());

  EXPECT_EQ(1, tester->GetNewNotificationCount());
  EXPECT_EQ(1, tester->GetNotificationCount());
  EXPECT_EQ(0, tester->GetStickyNotificationCount());
  EXPECT_EQ(NotificationPanel::STICKY_AND_NEW, tester->state());

  collection->Add(NewMockNotification("2"), browser()->profile());

  EXPECT_EQ(2, tester->GetNewNotificationCount());
  EXPECT_EQ(2, tester->GetNotificationCount());
  EXPECT_EQ(NotificationPanel::STICKY_AND_NEW, tester->state());

  collection->Remove(NewMockNotification("1"));
  RunAllPending();

  EXPECT_EQ(1, tester->GetNewNotificationCount());
  EXPECT_EQ(1, tester->GetNotificationCount());
  EXPECT_EQ(NotificationPanel::STICKY_AND_NEW, tester->state());

  collection->Remove(NewMockNotification("2"));
  RunAllPending();
  EXPECT_EQ(0, tester->GetNewNotificationCount());
  EXPECT_EQ(0, tester->GetNotificationCount());
  EXPECT_EQ(NotificationPanel::CLOSED, tester->state());
}

IN_PROC_BROWSER_TEST_F(NotificationTest, TestMouseMotion) {
  BalloonCollectionImpl* collection = GetBalloonCollectionImpl();
  NotificationPanel* panel = GetNotificationPanel();
  NotificationPanelTester* tester = panel->GetTester();

  // Using system notification as regular notification.
  collection->Add(NewMockNotification("1"), browser()->profile());
  collection->Add(NewMockNotification("2"), browser()->profile());

  EXPECT_EQ(NotificationPanel::STICKY_AND_NEW, tester->state());

  panel->OnMouseMotion();
  EXPECT_EQ(NotificationPanel::KEEP_SIZE, tester->state());

  collection->Remove(NewMockNotification("1"));
  RunAllPending();

  EXPECT_EQ(1, tester->GetNewNotificationCount());
  EXPECT_EQ(1, tester->GetNotificationCount());
  EXPECT_EQ(NotificationPanel::KEEP_SIZE, tester->state());

  collection->Remove(NewMockNotification("2"));
  RunAllPending();
  EXPECT_EQ(0, tester->GetNotificationCount());
  EXPECT_EQ(NotificationPanel::CLOSED, tester->state());

  collection->Add(NewMockNotification("3"), browser()->profile());
  EXPECT_EQ(NotificationPanel::STICKY_AND_NEW, tester->state());
  collection->Remove(NewMockNotification("3"));

  RunAllPending();
  EXPECT_EQ(0, tester->GetNotificationCount());
  EXPECT_EQ(NotificationPanel::CLOSED, tester->state());
}

IN_PROC_BROWSER_TEST_F(NotificationTest, TestSystemNotification) {
  BalloonCollectionImpl* collection = GetBalloonCollectionImpl();
  NotificationPanel* panel = GetNotificationPanel();
  scoped_refptr<MockNotificationDelegate> delegate(
      new MockNotificationDelegate("power"));
  NotificationPanelTester* tester = panel->GetTester();

  Notification notify = NewMockNotification(delegate.get());
  collection->AddSystemNotification(notify, browser()->profile(), true, false);

  EXPECT_EQ(1, tester->GetNewNotificationCount());
  EXPECT_EQ(1, tester->GetStickyNotificationCount());

  Notification update = SystemNotificationFactory::Create(
      GURL(), ASCIIToUTF16("Title"), ASCIIToUTF16("updated"), delegate.get());
  collection->UpdateNotification(update);

  EXPECT_EQ(1, tester->GetStickyNotificationCount());

  // Dismiss the notification.
  // TODO(oshima): Consider updating API to Remove(NotificationDelegate)
  // or Remove(std::string id);
  collection->Remove(Notification(GURL(), GURL(),
                                  std::wstring(), delegate.get()));
  RunAllPending();

  EXPECT_EQ(0, tester->GetStickyNotificationCount());
  EXPECT_EQ(0, tester->GetNewNotificationCount());
  // TODO(oshima): check content, etc..
}

}  // namespace chromeos
