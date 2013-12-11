// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_vector.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_delegate.h"
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service.h"
#include "chrome/browser/notifications/sync_notifier/sync_notifier_test_utils.h"
#include "chrome/browser/notifications/sync_notifier/synced_notification.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "sync/api/sync_change.h"

const char kTestNotificationId[] = "SomeRandomNotificationId";

class StubChromeNotifierService : public notifier::ChromeNotifierService {
 public:
  explicit StubChromeNotifierService(Profile* profile)
      : ChromeNotifierService(profile, NULL) {}

  virtual ~StubChromeNotifierService() {}

  virtual void MarkNotificationAsRead(const std::string& id) OVERRIDE {
    read_id_ = id;
  }

  notifier::SyncedNotification* CreateNotification(
      const std::string& title,
      const std::string& text,
      const std::string& app_icon_url,
      const std::string& image_url,
      const std::string& app_id,
      const std::string& key,
      sync_pb::CoalescedSyncedNotification_ReadState read_state) {
    syncer::SyncData sync_data = CreateSyncData(title, text, app_icon_url,
                                                image_url,app_id, key,
                                                read_state);
    // Set enough fields in sync_data, including specifics, for our tests
    // to pass.
    notifier::SyncedNotification* notification =
        new notifier::SyncedNotification(sync_data);
    // Retain ownership to avoid memory leaks in tests.
    owned_notifications_.push_back(notification);
    return notification;
  }

  // For testing, just return our test notification no matter what key the
  // caller sends.
  virtual notifier::SyncedNotification* FindNotificationById(
      const std::string& id) OVERRIDE {
    return CreateNotification(
        kTitle1, kText1, kIconUrl1, kImageUrl1, kAppId1, kKey1, kUnread);
  }

  const std::string& read_id() { return read_id_; }

 private:
  std::string read_id_;
  ScopedVector<notifier::SyncedNotification> owned_notifications_;
};

class ChromeNotifierDelegateTest : public BrowserWithTestWindowTest {
 public:
  ChromeNotifierDelegateTest() {}
  virtual ~ChromeNotifierDelegateTest() {}

  virtual void SetUp() OVERRIDE {
    BrowserWithTestWindowTest::SetUp();
    notifier_.reset(new StubChromeNotifierService(profile()));
  }

  virtual void TearDown() OVERRIDE {
    notifier_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  StubChromeNotifierService* notifier() { return notifier_.get(); }

 private:
  scoped_ptr<StubChromeNotifierService> notifier_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNotifierDelegateTest);
};

TEST_F(ChromeNotifierDelegateTest, ClickTest) {
  std::string id(kTestNotificationId);
  scoped_refptr<notifier::ChromeNotifierDelegate> delegate(
      new notifier::ChromeNotifierDelegate(id, notifier()));

  // Set up an observer to wait for the navigation
  content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_TAB_ADDED,
        content::NotificationService::AllSources());

  delegate->Click();

  // Wait for navigation to finish.
  observer.Wait();

  // Verify the navigation happened as expected - we should be on chrome://flags
  GURL url(kDefaultDestinationUrl);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(url, tab->GetController().GetActiveEntry()->GetVirtualURL());
}

TEST_F(ChromeNotifierDelegateTest, ButtonClickTest) {
  std::string id(kTestNotificationId);
  scoped_refptr<notifier::ChromeNotifierDelegate> delegate(
      new notifier::ChromeNotifierDelegate(id, notifier()));

  // Set up an observer to wait for the navigation
  content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_TAB_ADDED,
        content::NotificationService::AllSources());

  delegate->ButtonClick(0);

  // Wait for navigation to finish.
  observer.Wait();

  // Verify the navigation happened as expected - we should be on chrome://sync
  content::WebContents* tab;
  GURL url1(kButtonOneUrl);
  tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(url1, tab->GetController().GetActiveEntry()->GetVirtualURL());


  delegate->ButtonClick(1);

  // Wait for navigation to finish.
  observer.Wait();

  // Verify the navigation happened as expected - we should be on chrome://sync
  GURL url2(kButtonTwoUrl);
  tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(url2, tab->GetController().GetActiveEntry()->GetVirtualURL());
}

TEST_F(ChromeNotifierDelegateTest, CloseTest) {
  std::string id(kTestNotificationId);
  scoped_refptr<notifier::ChromeNotifierDelegate> delegate(
      new notifier::ChromeNotifierDelegate(id, notifier()));

  delegate->Close(false);
  ASSERT_EQ("", notifier()->read_id());

  delegate->Close(true);
  ASSERT_EQ(kTestNotificationId, notifier()->read_id());
}
