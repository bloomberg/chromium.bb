// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_delegate.h"
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service.h"
#include "chrome/browser/notifications/sync_notifier/sync_notifier_test_utils.h"
#include "chrome/browser/notifications/sync_notifier/synced_notification.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/test_utils.h"
#include "sync/api/sync_change.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/notification_types.h"

const char kTestNotificationId[] = "SomeRandomNotificationId";

class StubChromeNotifierService : public notifier::ChromeNotifierService {
 public:
  StubChromeNotifierService()
      : ChromeNotifierService(ProfileManager::GetDefaultProfile(), NULL) {}

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
    return new notifier::SyncedNotification(sync_data);
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
};

class ChromeNotifierDelegateBrowserTest : public InProcessBrowserTest {};

// Test will not have access to the browser profile on linux aura
#if defined(OS_LINUX) && defined(USE_AURA)
#define MAYBE_ClickTest \
    DISABLED_ClickTest
#else
#define MAYBE_ClickTest \
    ClickTest
#endif

IN_PROC_BROWSER_TEST_F(ChromeNotifierDelegateBrowserTest, MAYBE_ClickTest) {
  std::string id(kTestNotificationId);
  StubChromeNotifierService notifier;
  notifier::ChromeNotifierDelegate* delegate =
      new notifier::ChromeNotifierDelegate(id, &notifier);

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

// Test will not have access to the browser profile on linux aura.
#if defined(OS_LINUX) && defined(USE_AURA)
#define MAYBE_ButtonClickTest \
    DISABLED_ButtonClickTest
#else
#define MAYBE_ButtonClickTest \
    ButtonClickTest
#endif

IN_PROC_BROWSER_TEST_F(ChromeNotifierDelegateBrowserTest,
                       MAYBE_ButtonClickTest) {
  std::string id(kTestNotificationId);
  StubChromeNotifierService notifier;
  notifier::ChromeNotifierDelegate* delegate =
      new notifier::ChromeNotifierDelegate(id, &notifier);

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

  // Also verify that the click dismissed the notification.
  ASSERT_EQ(kTestNotificationId, notifier.read_id());
}

IN_PROC_BROWSER_TEST_F(ChromeNotifierDelegateBrowserTest, CloseTest) {
  std::string id(kTestNotificationId);
  StubChromeNotifierService notifier;
  notifier::ChromeNotifierDelegate* delegate =
      new notifier::ChromeNotifierDelegate(id, &notifier);

  delegate->Close(false);
  ASSERT_EQ("", notifier.read_id());

  delegate->Close(true);
  ASSERT_EQ(kTestNotificationId, notifier.read_id());
}
