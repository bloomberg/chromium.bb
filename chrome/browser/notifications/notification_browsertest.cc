// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/balloon_collection.h"
#include "chrome/browser/notifications/balloon_host.h"
#include "chrome/browser/notifications/balloon_notification_ui_manager.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_pattern.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/base/net_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/message_center_switches.h"
#include "ui/message_center/message_center_util.h"
#include "url/gurl.h"

namespace {

const char kExpectedIconUrl[] = "/notifications/no_such_file.png";

enum InfobarAction {
  DISMISS = 0,
  ALLOW,
  DENY,
};

class NotificationChangeObserver {
public:
  virtual ~NotificationChangeObserver() {}
  virtual bool Wait() = 0;
};

class MessageCenterChangeObserver
    : public message_center::MessageCenterObserver,
      public NotificationChangeObserver {
 public:
  MessageCenterChangeObserver()
      : notification_received_(false) {
    message_center::MessageCenter::Get()->AddObserver(this);
  }

  virtual ~MessageCenterChangeObserver() {
    message_center::MessageCenter::Get()->RemoveObserver(this);
  }

  // NotificationChangeObserver:
  virtual bool Wait() OVERRIDE {
    if (notification_received_)
      return true;

    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
    return notification_received_;
  }

  // message_center::MessageCenterObserver:
  virtual void OnNotificationAdded(
      const std::string& notification_id) OVERRIDE {
    OnMessageCenterChanged();
  }

  virtual void OnNotificationRemoved(const std::string& notification_id,
                                     bool by_user) OVERRIDE {
    OnMessageCenterChanged();
  }

  virtual void OnNotificationUpdated(
      const std::string& notification_id) OVERRIDE {
    OnMessageCenterChanged();
  }

  void OnMessageCenterChanged() {
    notification_received_ = true;
    if (message_loop_runner_.get())
      message_loop_runner_->Quit();
  }

  bool notification_received_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterChangeObserver);
};

class NotificationBalloonChangeObserver
    : public content::NotificationObserver,
      public NotificationChangeObserver {
 public:
  NotificationBalloonChangeObserver()
      : collection_(BalloonNotificationUIManager::GetInstanceForTesting()->
            balloon_collection()),
        collection_changed_(false),
        notification_received_(false),
        running_(false),
        done_(false) {
    registrar_.Add(this, chrome::NOTIFICATION_NOTIFY_BALLOON_CONNECTED,
                   content::NotificationService::AllSources());
    registrar_.Add(this, chrome::NOTIFICATION_NOTIFY_BALLOON_DISCONNECTED,
                   content::NotificationService::AllSources());
    collection_->set_on_collection_changed_callback(
        base::Bind(&NotificationBalloonChangeObserver::OnCollectionChanged,
                   base::Unretained(this)));
  }

  virtual ~NotificationBalloonChangeObserver() {
    collection_->set_on_collection_changed_callback(base::Closure());
  }

  // NotificationChangeObserver:
  virtual bool Wait() OVERRIDE {
    if (!Check()) {
      running_ = true;
      message_loop_runner_ = new content::MessageLoopRunner;
      message_loop_runner_->Run();
      EXPECT_TRUE(done_);
    }
    return done_;
  }

  bool Check() {
    if (done_)
      return true;

    if (collection_changed_ && notification_received_) {
      done_ = true;
      if (running_) {
        message_loop_runner_->Quit();
        running_ = false;
      }
    }
    return done_;
  }

  void OnCollectionChanged() {
    collection_changed_ = true;
    Check();
  }

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    DCHECK(type == chrome::NOTIFICATION_NOTIFY_BALLOON_DISCONNECTED ||
           type == chrome::NOTIFICATION_NOTIFY_BALLOON_CONNECTED);
    notification_received_ = true;
    Check();
  }

 private:
  content::NotificationRegistrar registrar_;
  BalloonCollection* collection_;

  bool collection_changed_;
  bool notification_received_;
  bool running_;
  bool done_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(NotificationBalloonChangeObserver);
};

}  // namespace

class NotificationsTest : public InProcessBrowserTest {
 public:
  NotificationsTest() {}

 protected:
  int GetNotificationCount();

  NotificationChangeObserver* CreateObserver();

  void CloseBrowserWindow(Browser* browser);
  void CrashTab(Browser* browser, int index);
  const std::deque<Balloon*>& GetActiveBalloons();
  void CrashNotification(Balloon* balloon);
  bool CloseNotificationAndWait(const Notification& notification);

  void SetDefaultPermissionSetting(ContentSetting setting);
  void DenyOrigin(const GURL& origin);
  void AllowOrigin(const GURL& origin);
  void AllowAllOrigins();

  void VerifyInfoBar(const Browser* browser, int index);
  std::string CreateNotification(Browser* browser,
                                 bool wait_for_new_balloon,
                                 const char* icon,
                                 const char* title,
                                 const char* body,
                                 const char* replace_id);
  std::string CreateSimpleNotification(Browser* browser,
                                       bool wait_for_new_balloon);
  bool RequestPermissionAndWait(Browser* browser);
  bool CancelNotification(const char* notification_id, Browser* browser);
  bool PerformActionOnInfoBar(Browser* browser,
                              InfobarAction action,
                              size_t infobar_index,
                              int tab_index);
  void GetPrefsByContentSetting(ContentSetting setting,
                                ContentSettingsForOneType* settings);
  bool CheckOriginInSetting(const ContentSettingsForOneType& settings,
                            const GURL& origin);

  GURL GetTestPageURL() const {
    return embedded_test_server()->GetURL(
      "/notifications/notification_tester.html");
  }

 private:
  void DropOriginPreference(const GURL& origin);
  DesktopNotificationService* GetDesktopNotificationService();
};

int NotificationsTest::GetNotificationCount() {
  if (message_center::IsRichNotificationEnabled()) {
    return message_center::MessageCenter::Get()->NotificationCount();
  } else {
    return BalloonNotificationUIManager::GetInstanceForTesting()->
           balloon_collection()->GetActiveBalloons().size();
  }
}

NotificationChangeObserver* NotificationsTest::CreateObserver() {
  if (message_center::IsRichNotificationEnabled())
    return new MessageCenterChangeObserver();
  else
    return new NotificationBalloonChangeObserver();
}

void NotificationsTest::CloseBrowserWindow(Browser* browser) {
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::Source<Browser>(browser));
  browser->window()->Close();
  observer.Wait();
}

void NotificationsTest::CrashTab(Browser* browser, int index) {
  content::CrashTab(browser->tab_strip_model()->GetWebContentsAt(index));
}

const std::deque<Balloon*>& NotificationsTest::GetActiveBalloons() {
  return BalloonNotificationUIManager::GetInstanceForTesting()->
      balloon_collection()->GetActiveBalloons();
}

void NotificationsTest::CrashNotification(Balloon* balloon) {
  content::CrashTab(balloon->balloon_view()->GetHost()->web_contents());
}

bool NotificationsTest::CloseNotificationAndWait(
    const Notification& notification) {
  scoped_ptr<NotificationChangeObserver> observer(CreateObserver());
  bool success = g_browser_process->notification_ui_manager()->
      CancelById(notification.notification_id());
  if (success)
    return observer->Wait();
  return false;
}

void NotificationsTest::SetDefaultPermissionSetting(ContentSetting setting) {
  DesktopNotificationService* service = GetDesktopNotificationService();
  service->SetDefaultContentSetting(setting);
}

void NotificationsTest::DenyOrigin(const GURL& origin) {
  DropOriginPreference(origin);
  GetDesktopNotificationService()->DenyPermission(origin);
}

void NotificationsTest::AllowOrigin(const GURL& origin) {
  DropOriginPreference(origin);
  GetDesktopNotificationService()->GrantPermission(origin);
}

void NotificationsTest::AllowAllOrigins() {
  GetDesktopNotificationService()->ResetAllOrigins();
  GetDesktopNotificationService()->SetDefaultContentSetting(
      CONTENT_SETTING_ALLOW);
}

void NotificationsTest::VerifyInfoBar(const Browser* browser, int index) {
  InfoBarService* infobar_service = InfoBarService::FromWebContents(
      browser->tab_strip_model()->GetWebContentsAt(index));

  ASSERT_EQ(1U, infobar_service->infobar_count());
  ConfirmInfoBarDelegate* confirm_infobar =
      infobar_service->infobar_at(0)->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(confirm_infobar);
  int buttons = confirm_infobar->GetButtons();
  EXPECT_TRUE(buttons & ConfirmInfoBarDelegate::BUTTON_OK);
  EXPECT_TRUE(buttons & ConfirmInfoBarDelegate::BUTTON_CANCEL);
}

std::string NotificationsTest::CreateNotification(
    Browser* browser,
    bool wait_for_new_balloon,
    const char* icon,
    const char* title,
    const char* body,
    const char* replace_id) {
  std::string script = base::StringPrintf(
      "createNotification('%s', '%s', '%s', '%s');",
      icon, title, body, replace_id);

  scoped_ptr<NotificationChangeObserver> observer(CreateObserver());
  std::string result;
  bool success = content::ExecuteScriptAndExtractString(
      browser->tab_strip_model()->GetActiveWebContents(),
      script,
      &result);
  if (success && result != "-1" && wait_for_new_balloon)
    success = observer->Wait();
  EXPECT_TRUE(success);

  return result;
}

std::string NotificationsTest::CreateSimpleNotification(
    Browser* browser,
    bool wait_for_new_balloon) {
  return CreateNotification(
      browser, wait_for_new_balloon,
      "no_such_file.png", "My Title", "My Body", "");
}

bool NotificationsTest::RequestPermissionAndWait(Browser* browser) {
  InfoBarService* infobar_service = InfoBarService::FromWebContents(
      browser->tab_strip_model()->GetActiveWebContents());
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
      content::Source<InfoBarService>(infobar_service));
  std::string result;
  bool success = content::ExecuteScriptAndExtractString(
      browser->tab_strip_model()->GetActiveWebContents(),
      "requestPermission();",
      &result);
  if (!success || result != "1")
    return false;
  observer.Wait();
  return true;
}

bool NotificationsTest::CancelNotification(
    const char* notification_id,
    Browser* browser) {
  std::string script = base::StringPrintf(
      "cancelNotification('%s');",
      notification_id);

  scoped_ptr<NotificationChangeObserver> observer(CreateObserver());
  std::string result;
  bool success = content::ExecuteScriptAndExtractString(
      browser->tab_strip_model()->GetActiveWebContents(),
      script,
      &result);
  if (!success || result != "1")
    return false;
  return observer->Wait();
}

bool NotificationsTest::PerformActionOnInfoBar(
    Browser* browser,
    InfobarAction action,
    size_t infobar_index,
    int tab_index) {
  InfoBarService* infobar_service = InfoBarService::FromWebContents(
      browser->tab_strip_model()->GetWebContentsAt(tab_index));
  if (infobar_index >= infobar_service->infobar_count()) {
    ADD_FAILURE();
    return false;
  }

  InfoBar* infobar = infobar_service->infobar_at(infobar_index);
  InfoBarDelegate* infobar_delegate = infobar->delegate();
  switch (action) {
    case DISMISS:
      infobar_delegate->InfoBarDismissed();
      infobar_service->RemoveInfoBar(infobar);
      return true;

    case ALLOW: {
      ConfirmInfoBarDelegate* confirm_infobar_delegate =
          infobar_delegate->AsConfirmInfoBarDelegate();
      if (!confirm_infobar_delegate) {
        ADD_FAILURE();
      } else if (confirm_infobar_delegate->Accept()) {
        infobar_service->RemoveInfoBar(infobar);
        return true;
      }
    }

    case DENY: {
      ConfirmInfoBarDelegate* confirm_infobar_delegate =
          infobar_delegate->AsConfirmInfoBarDelegate();
      if (!confirm_infobar_delegate) {
        ADD_FAILURE();
      } else if (confirm_infobar_delegate->Cancel()) {
        infobar_service->RemoveInfoBar(infobar);
        return true;
      }
    }
  }

  return false;
}

void NotificationsTest::GetPrefsByContentSetting(
    ContentSetting setting,
    ContentSettingsForOneType* settings) {
  DesktopNotificationService* service = GetDesktopNotificationService();
  service->GetNotificationsSettings(settings);
  for (ContentSettingsForOneType::iterator it = settings->begin();
       it != settings->end(); ) {
    if (it->setting != setting || it->source.compare("preference") != 0)
      it = settings->erase(it);
    else
      ++it;
  }
}

bool NotificationsTest::CheckOriginInSetting(
    const ContentSettingsForOneType& settings,
    const GURL& origin) {
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURLNoWildcard(origin);
  for (ContentSettingsForOneType::const_iterator it = settings.begin();
       it != settings.end(); ++it) {
    if (it->primary_pattern == pattern)
      return true;
  }
  return false;
}

void NotificationsTest::DropOriginPreference(const GURL& origin) {
  GetDesktopNotificationService()->ClearSetting(
      ContentSettingsPattern::FromURLNoWildcard(origin));
}

DesktopNotificationService* NotificationsTest::GetDesktopNotificationService() {
  Profile* profile = browser()->profile();
  return DesktopNotificationServiceFactory::GetForProfile(profile);
}

// If this flakes, use http://crbug.com/62311 and http://crbug.com/74428.
IN_PROC_BROWSER_TEST_F(NotificationsTest, TestUserGestureInfobar) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/notifications/notifications_request_function.html"));

  // Request permission by calling request() while eval'ing an inline script;
  // That's considered a user gesture to webkit, and should produce an infobar.
  bool result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(request());",
      &result));
  EXPECT_TRUE(result);

  InfoBarService* infobar_service = InfoBarService::FromWebContents(
      browser()->tab_strip_model()->GetWebContentsAt(0));
  EXPECT_EQ(1U, infobar_service->infobar_count());
}

// If this flakes, use http://crbug.com/62311.
IN_PROC_BROWSER_TEST_F(NotificationsTest, TestNoUserGestureInfobar) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Load a page which just does a request; no user gesture should result
  // in no infobar.
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/notifications/notifications_request_inline.html"));

  InfoBarService* infobar_service = InfoBarService::FromWebContents(
      browser()->tab_strip_model()->GetWebContentsAt(0));
  EXPECT_EQ(0U, infobar_service->infobar_count());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestCreateSimpleNotification) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Creates a simple notification.
  AllowAllOrigins();
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());

  std::string result = CreateSimpleNotification(browser(), true);
  EXPECT_NE("-1", result);

  GURL EXPECTED_ICON_URL = embedded_test_server()->GetURL(kExpectedIconUrl);
  ASSERT_EQ(1, GetNotificationCount());
  if (message_center::IsRichNotificationEnabled()) {
    message_center::NotificationList::Notifications notifications =
        message_center::MessageCenter::Get()->GetVisibleNotifications();
    EXPECT_EQ(base::ASCIIToUTF16("My Title"),
              (*notifications.rbegin())->title());
    EXPECT_EQ(base::ASCIIToUTF16("My Body"),
              (*notifications.rbegin())->message());
  } else {
    const std::deque<Balloon*>& balloons = GetActiveBalloons();
    ASSERT_EQ(1U, balloons.size());
    Balloon* balloon = balloons[0];
    const Notification& notification = balloon->notification();
    EXPECT_EQ(EXPECTED_ICON_URL, notification.icon_url());
    EXPECT_EQ(base::ASCIIToUTF16("My Title"), notification.title());
    EXPECT_EQ(base::ASCIIToUTF16("My Body"), notification.message());
  }
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestCloseNotification) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Creates a notification and closes it.
  AllowAllOrigins();
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());

  std::string result = CreateSimpleNotification(browser(), true);
  EXPECT_NE("-1", result);
  ASSERT_EQ(1, GetNotificationCount());

  if (message_center::IsRichNotificationEnabled()) {
    message_center::NotificationList::Notifications notifications =
        message_center::MessageCenter::Get()->GetVisibleNotifications();
    message_center::MessageCenter::Get()->RemoveNotification(
        (*notifications.rbegin())->id(),
        true);  // by_user
  } else {
    const std::deque<Balloon*>& balloons = GetActiveBalloons();
    EXPECT_TRUE(CloseNotificationAndWait(balloons[0]->notification()));
  }

  ASSERT_EQ(0, GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestCancelNotification) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Creates a notification and cancels it in the origin page.
  AllowAllOrigins();
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());

  std::string note_id = CreateSimpleNotification(browser(), true);
  EXPECT_NE(note_id, "-1");

  ASSERT_EQ(1, GetNotificationCount());
  ASSERT_TRUE(CancelNotification(note_id.c_str(), browser()));
  ASSERT_EQ(0, GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestPermissionInfobarAppears) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Requests notification privileges and verifies the infobar appears.
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());
  ASSERT_TRUE(RequestPermissionAndWait(browser()));

  ASSERT_EQ(0, GetNotificationCount());
  ASSERT_NO_FATAL_FAILURE(VerifyInfoBar(browser(), 0));
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestAllowOnPermissionInfobar) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Tries to create a notification and clicks allow on the infobar.
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());
  // This notification should not be shown because we do not have permission.
  CreateSimpleNotification(browser(), false);
  ASSERT_EQ(0, GetNotificationCount());

  ASSERT_TRUE(RequestPermissionAndWait(browser()));
  ASSERT_TRUE(PerformActionOnInfoBar(browser(), ALLOW, 0, 0));

  CreateSimpleNotification(browser(), true);
  EXPECT_EQ(1, GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestDenyOnPermissionInfobar) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Test that no notification is created
  // when Deny is chosen from permission infobar.
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());
  ASSERT_TRUE(RequestPermissionAndWait(browser()));
  PerformActionOnInfoBar(browser(), DENY, 0, 0);
  CreateSimpleNotification(browser(), false);
  ASSERT_EQ(0, GetNotificationCount());
  ContentSettingsForOneType settings;
  GetPrefsByContentSetting(CONTENT_SETTING_BLOCK, &settings);
  EXPECT_TRUE(CheckOriginInSetting(settings, GetTestPageURL()));
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestClosePermissionInfobar) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Test that no notification is created when permission infobar is dismissed.
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());
  ASSERT_TRUE(RequestPermissionAndWait(browser()));
  PerformActionOnInfoBar(browser(), DISMISS, 0, 0);
  CreateSimpleNotification(browser(), false);
  ASSERT_EQ(0, GetNotificationCount());
  ContentSettingsForOneType settings;
  GetPrefsByContentSetting(CONTENT_SETTING_BLOCK, &settings);
  EXPECT_EQ(0U, settings.size());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestAllowNotificationsFromAllSites) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Verify that all domains can be allowed to show notifications.
  SetDefaultPermissionSetting(CONTENT_SETTING_ALLOW);
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());

  std::string result = CreateSimpleNotification(browser(), true);
  EXPECT_NE("-1", result);

  ASSERT_EQ(1, GetNotificationCount());
  InfoBarService* infobar_service = InfoBarService::FromWebContents(
      browser()->tab_strip_model()->GetWebContentsAt(0));
  EXPECT_EQ(0U, infobar_service->infobar_count());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestDenyNotificationsFromAllSites) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Verify that no domain can show notifications.
  SetDefaultPermissionSetting(CONTENT_SETTING_BLOCK);
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());

  std::string result = CreateSimpleNotification(browser(), false);
  EXPECT_EQ("-1", result);

  ASSERT_EQ(0, GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestDenyDomainAndAllowAll) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Verify that denying a domain and allowing all shouldn't show
  // notifications from the denied domain.
  DenyOrigin(GetTestPageURL().GetOrigin());
  SetDefaultPermissionSetting(CONTENT_SETTING_ALLOW);

  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());

  std::string result = CreateSimpleNotification(browser(), false);
  EXPECT_EQ("-1", result);

  ASSERT_EQ(0, GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestAllowDomainAndDenyAll) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Verify that allowing a domain and denying all others should show
  // notifications from the allowed domain.
  AllowOrigin(GetTestPageURL().GetOrigin());
  SetDefaultPermissionSetting(CONTENT_SETTING_BLOCK);

  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());

  std::string result = CreateSimpleNotification(browser(), true);
  EXPECT_NE("-1", result);

  ASSERT_EQ(1, GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestDenyAndThenAllowDomain) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Verify that denying and again allowing should show notifications.
  DenyOrigin(GetTestPageURL().GetOrigin());

  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());

  std::string result = CreateSimpleNotification(browser(), false);
  EXPECT_EQ("-1", result);

  ASSERT_EQ(0, GetNotificationCount());

  AllowOrigin(GetTestPageURL().GetOrigin());
  result = CreateSimpleNotification(browser(), true);
  EXPECT_NE("-1", result);

  ASSERT_EQ(1, GetNotificationCount());
  InfoBarService* infobar_service = InfoBarService::FromWebContents(
      browser()->tab_strip_model()->GetWebContentsAt(0));
  EXPECT_EQ(0U, infobar_service->infobar_count());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestCreateDenyCloseNotifications) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Verify able to create, deny, and close the notification.
  AllowAllOrigins();
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());
  CreateSimpleNotification(browser(), true);
  ASSERT_EQ(1, GetNotificationCount());

  DenyOrigin(GetTestPageURL().GetOrigin());
  ContentSettingsForOneType settings;
  GetPrefsByContentSetting(CONTENT_SETTING_BLOCK, &settings);
  ASSERT_TRUE(CheckOriginInSetting(settings, GetTestPageURL().GetOrigin()));

  EXPECT_EQ(1, GetNotificationCount());
  if (message_center::IsRichNotificationEnabled()) {
    message_center::NotificationList::Notifications notifications =
        message_center::MessageCenter::Get()->GetVisibleNotifications();
    message_center::MessageCenter::Get()->RemoveNotification(
        (*notifications.rbegin())->id(),
        true);  // by_user
  } else {
    const std::deque<Balloon*>& balloons = GetActiveBalloons();
    ASSERT_TRUE(CloseNotificationAndWait(balloons[0]->notification()));
  }
  ASSERT_EQ(0, GetNotificationCount());
}

// Crashes on Linux/Win. See http://crbug.com/160657.
IN_PROC_BROWSER_TEST_F(
    NotificationsTest,
    DISABLED_TestOriginPrefsNotSavedInIncognito) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Verify that allow/deny origin preferences are not saved in incognito.
  Browser* incognito = CreateIncognitoBrowser();
  ui_test_utils::NavigateToURL(incognito, GetTestPageURL());
  ASSERT_TRUE(RequestPermissionAndWait(incognito));
  PerformActionOnInfoBar(incognito, DENY, 0, 0);
  CloseBrowserWindow(incognito);

  incognito = CreateIncognitoBrowser();
  ui_test_utils::NavigateToURL(incognito, GetTestPageURL());
  ASSERT_TRUE(RequestPermissionAndWait(incognito));
  PerformActionOnInfoBar(incognito, ALLOW, 0, 0);
  CreateSimpleNotification(incognito, true);
  ASSERT_EQ(1, GetNotificationCount());
  CloseBrowserWindow(incognito);

  incognito = CreateIncognitoBrowser();
  ui_test_utils::NavigateToURL(incognito, GetTestPageURL());
  ASSERT_TRUE(RequestPermissionAndWait(incognito));

  ContentSettingsForOneType settings;
  GetPrefsByContentSetting(CONTENT_SETTING_BLOCK, &settings);
  EXPECT_EQ(0U, settings.size());
  GetPrefsByContentSetting(CONTENT_SETTING_ALLOW, &settings);
  EXPECT_EQ(0U, settings.size());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestExitBrowserWithInfobar) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Exit the browser window, when the infobar appears.
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());
  ASSERT_TRUE(RequestPermissionAndWait(browser()));
}

// Times out on Windows and Linux. http://crbug.com/168976
#if defined(OS_WIN) || defined(OS_LINUX)
#define MAYBE_TestCrashTabWithPermissionInfobar \
    DISABLED_TestCrashTabWithPermissionInfobar
#else
#define MAYBE_TestCrashTabWithPermissionInfobar \
    TestCrashTabWithPermissionInfobar
#endif
IN_PROC_BROWSER_TEST_F(NotificationsTest,
                       MAYBE_TestCrashTabWithPermissionInfobar) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Test crashing the tab with permission infobar doesn't crash Chrome.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      embedded_test_server()->GetURL("/empty.html"),
      NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());
  ASSERT_TRUE(RequestPermissionAndWait(browser()));
  CrashTab(browser(), 0);
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestKillNotificationProcess) {
  // Notifications don't have their own process with the message center.
  if (message_center::IsRichNotificationEnabled())
    return;

  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Test killing a notification doesn't crash Chrome.
  AllowAllOrigins();
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());
  CreateSimpleNotification(browser(), true);
  ASSERT_EQ(1, GetNotificationCount());

  const std::deque<Balloon*>& balloons = GetActiveBalloons();
  ASSERT_EQ(1U, balloons.size());
  CrashNotification(balloons[0]);
  ASSERT_EQ(0, GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestIncognitoNotification) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Test notifications in incognito window.
  Browser* browser = CreateIncognitoBrowser();
  ui_test_utils::NavigateToURL(browser, GetTestPageURL());
  browser->tab_strip_model()->ActivateTabAt(0, true);
  ASSERT_TRUE(RequestPermissionAndWait(browser));
  PerformActionOnInfoBar(browser, ALLOW, 0, 0);
  CreateSimpleNotification(browser, true);
  ASSERT_EQ(1, GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestCloseTabWithPermissionInfobar) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Test that user can close tab when infobar present.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL("about:blank"),
      NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());
  ASSERT_TRUE(RequestPermissionAndWait(browser()));
  content::WebContentsDestroyedWatcher destroyed_watcher(
      browser()->tab_strip_model()->GetWebContentsAt(0));
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabStripModel::CLOSE_NONE);
  destroyed_watcher.Wait();
}

IN_PROC_BROWSER_TEST_F(
    NotificationsTest,
    TestNavigateAwayWithPermissionInfobar) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Test navigating away when an infobar is present,
  // then trying to create a notification from the same page.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL("about:blank"),
      NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());
  ASSERT_TRUE(RequestPermissionAndWait(browser()));
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());
  ASSERT_TRUE(RequestPermissionAndWait(browser()));
  PerformActionOnInfoBar(browser(), ALLOW, 0, 0);
  CreateSimpleNotification(browser(), true);
  ASSERT_EQ(1, GetNotificationCount());
}

// See crbug.com/248470
#if defined(OS_LINUX)
#define MAYBE_TestCrashRendererNotificationRemain \
    DISABLED_TestCrashRendererNotificationRemain
#else
#define MAYBE_TestCrashRendererNotificationRemain \
    TestCrashRendererNotificationRemain
#endif

IN_PROC_BROWSER_TEST_F(NotificationsTest,
                       MAYBE_TestCrashRendererNotificationRemain) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Test crashing renderer does not close or crash notification.
  AllowAllOrigins();
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL("about:blank"),
      NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());
  CreateSimpleNotification(browser(), true);
  ASSERT_EQ(1, GetNotificationCount());
  CrashTab(browser(), 0);
  ASSERT_EQ(1, GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestNotificationReplacement) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Test that we can replace a notification using the replaceId.
  AllowAllOrigins();

  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());

  std::string result = CreateNotification(
      browser(), true, "abc.png", "Title1", "Body1", "chat");
  EXPECT_NE("-1", result);

  ASSERT_EQ(1, GetNotificationCount());

  result = CreateNotification(
      browser(), false, "no_such_file.png", "Title2", "Body2", "chat");
  EXPECT_NE("-1", result);

  if (message_center::IsRichNotificationEnabled()) {
    ASSERT_EQ(1, GetNotificationCount());
    message_center::NotificationList::Notifications notifications =
        message_center::MessageCenter::Get()->GetVisibleNotifications();
    EXPECT_EQ(base::ASCIIToUTF16("Title2"), (*notifications.rbegin())->title());
    EXPECT_EQ(base::ASCIIToUTF16("Body2"),
              (*notifications.rbegin())->message());
  } else {
    const std::deque<Balloon*>& balloons = GetActiveBalloons();
    ASSERT_EQ(1U, balloons.size());
    Balloon* balloon = balloons[0];
    const Notification& notification = balloon->notification();
    GURL EXPECTED_ICON_URL = embedded_test_server()->GetURL(kExpectedIconUrl);
    EXPECT_EQ(EXPECTED_ICON_URL, notification.icon_url());
    EXPECT_EQ(base::ASCIIToUTF16("Title2"), notification.title());
    EXPECT_EQ(base::ASCIIToUTF16("Body2"), notification.message());
  }
}
