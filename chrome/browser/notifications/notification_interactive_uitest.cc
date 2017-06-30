// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_interactive_uitest_support.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/notification_blocker.h"
#include "url/gurl.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#endif

namespace {

class ToggledNotificationBlocker : public message_center::NotificationBlocker {
 public:
  ToggledNotificationBlocker()
      : message_center::NotificationBlocker(
            message_center::MessageCenter::Get()),
        notifications_enabled_(true) {}
  ~ToggledNotificationBlocker() override {}

  void SetNotificationsEnabled(bool enabled) {
    if (notifications_enabled_ != enabled) {
      notifications_enabled_ = enabled;
      NotifyBlockingStateChanged();
    }
  }

  // NotificationBlocker overrides:
  bool ShouldShowNotificationAsPopup(
      const message_center::Notification& notification) const override {
    return notifications_enabled_;
  }

 private:
  bool notifications_enabled_;

  DISALLOW_COPY_AND_ASSIGN(ToggledNotificationBlocker);
};

}  // namespace

namespace {

const char kExpectedIconUrl[] = "/notifications/no_such_file.png";

}  // namespace

// Flaky on Windows, Mac, Linux: http://crbug.com/437414.
IN_PROC_BROWSER_TEST_F(NotificationsTest, DISABLED_TestUserGestureInfobar) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/notifications/notifications_request_function.html"));

  // Request permission by calling request() while eval'ing an inline script;
  // That's considered a user gesture to webkit, and should produce an infobar.
  bool result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetActiveWebContents(browser()),
      "window.domAutomationController.send(request());", &result));
  EXPECT_TRUE(result);

  InfoBarService* infobar_service = InfoBarService::FromWebContents(
      browser()->tab_strip_model()->GetWebContentsAt(0));
  EXPECT_EQ(1U, infobar_service->infobar_count());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestCreateSimpleNotification) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Creates a simple notification.
  AllowAllOrigins();
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());

  std::string result = CreateSimpleNotification(browser(), true);
  EXPECT_NE("-1", result);

  GURL EXPECTED_ICON_URL = embedded_test_server()->GetURL(kExpectedIconUrl);
  ASSERT_EQ(1, GetNotificationCount());
  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  EXPECT_EQ(base::ASCIIToUTF16("My Title"),
            (*notifications.rbegin())->title());
  EXPECT_EQ(base::ASCIIToUTF16("My Body"),
            (*notifications.rbegin())->message());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, NotificationBlockerTest) {
  ToggledNotificationBlocker blocker;

  ASSERT_TRUE(embedded_test_server()->Start());

  // Creates a simple notification.
  AllowAllOrigins();
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());

  std::string result = CreateSimpleNotification(browser(), true);
  EXPECT_NE("-1", result);
  result = CreateSimpleNotification(browser(), true);
  EXPECT_NE("-1", result);

  blocker.SetNotificationsEnabled(false);
  EXPECT_EQ(0, GetNotificationPopupCount());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestCloseNotification) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Creates a notification and closes it.
  AllowAllOrigins();
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());

  std::string result = CreateSimpleNotification(browser(), true);
  EXPECT_NE("-1", result);
  ASSERT_EQ(1, GetNotificationCount());

  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  message_center::MessageCenter::Get()->RemoveNotification(
      (*notifications.rbegin())->id(),
      true);  // by_user

  ASSERT_EQ(0, GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestCancelNotification) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Creates a notification and cancels it in the origin page.
  AllowAllOrigins();
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());

  std::string note_id = CreateSimpleNotification(browser(), true);
  EXPECT_NE(note_id, "-1");

  ASSERT_EQ(1, GetNotificationCount());
  ASSERT_TRUE(CancelNotification(note_id.c_str(), browser()));
  ASSERT_EQ(0, GetNotificationCount());
}

// Requests notification privileges and verifies the prompt appears.
IN_PROC_BROWSER_TEST_F(NotificationsTest, TestPermissionRequestUIAppears) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());
  EXPECT_TRUE(RequestPermissionAndWait(browser()));
  ASSERT_EQ(0, GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestAllowOnPermissionRequestUI) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Tries to create a notification & clicks 'allow' on the prompt.
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());
  // This notification should not be shown because we do not have permission.
  CreateSimpleNotification(browser(), false);
  ASSERT_EQ(0, GetNotificationCount());

  ASSERT_TRUE(RequestAndAcceptPermission(browser()));

  CreateSimpleNotification(browser(), true);
  EXPECT_EQ(1, GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestDenyOnPermissionRequestUI) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Test that no notification is created when Deny is chosen from prompt.
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());
  ASSERT_TRUE(RequestAndDenyPermission(browser()));
  CreateSimpleNotification(browser(), false);
  ASSERT_EQ(0, GetNotificationCount());
  ContentSettingsForOneType settings;
  GetPrefsByContentSetting(CONTENT_SETTING_BLOCK, &settings);
  EXPECT_TRUE(CheckOriginInSetting(settings, GetTestPageURL()));
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestClosePermissionRequestUI) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Test that no notification is created when prompt is dismissed.
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());
  ASSERT_TRUE(RequestAndDismissPermission(browser()));
  CreateSimpleNotification(browser(), false);
  ASSERT_EQ(0, GetNotificationCount());
  ContentSettingsForOneType settings;
  GetPrefsByContentSetting(CONTENT_SETTING_BLOCK, &settings);
  EXPECT_EQ(0U, settings.size());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestPermissionAPI) {
  base::test::ScopedFeatureList scoped_feature_list;
  EnablePermissionsEmbargo(&scoped_feature_list);
  ASSERT_TRUE(embedded_test_server()->Start());

  // Test that Notification.permission returns the right thing.
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());
  EXPECT_EQ("default", QueryPermissionStatus(browser()));

  AllowOrigin(GetTestPageURL().GetOrigin());
  EXPECT_EQ("granted", QueryPermissionStatus(browser()));

  DenyOrigin(GetTestPageURL().GetOrigin());
  EXPECT_EQ("denied", QueryPermissionStatus(browser()));

  DropOriginPreference(GetTestPageURL().GetOrigin());

  // Verify embargo behaviour - automatically blocked after 3 dismisses.
  ASSERT_TRUE(RequestAndDismissPermission(browser()));
  EXPECT_EQ("default", QueryPermissionStatus(browser()));

  ASSERT_TRUE(RequestAndDismissPermission(browser()));
  EXPECT_EQ("default", QueryPermissionStatus(browser()));

  ASSERT_TRUE(RequestAndDismissPermission(browser()));
  EXPECT_EQ("denied", QueryPermissionStatus(browser()));
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestAllowNotificationsFromAllSites) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Verify that all domains can be allowed to show notifications.
  SetDefaultContentSetting(CONTENT_SETTING_ALLOW);
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());

  std::string result = CreateSimpleNotification(browser(), true);
  EXPECT_NE("-1", result);

  ASSERT_EQ(1, GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestDenyNotificationsFromAllSites) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Verify that no domain can show notifications.
  SetDefaultContentSetting(CONTENT_SETTING_BLOCK);
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());

  std::string result = CreateSimpleNotification(browser(), false);
  EXPECT_EQ("-1", result);

  ASSERT_EQ(0, GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestDenyDomainAndAllowAll) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Verify that denying a domain and allowing all shouldn't show
  // notifications from the denied domain.
  DenyOrigin(GetTestPageURL().GetOrigin());
  SetDefaultContentSetting(CONTENT_SETTING_ALLOW);

  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());

  std::string result = CreateSimpleNotification(browser(), false);
  EXPECT_EQ("-1", result);

  ASSERT_EQ(0, GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestAllowDomainAndDenyAll) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Verify that allowing a domain and denying all others should show
  // notifications from the allowed domain.
  AllowOrigin(GetTestPageURL().GetOrigin());
  SetDefaultContentSetting(CONTENT_SETTING_BLOCK);

  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());

  std::string result = CreateSimpleNotification(browser(), true);
  EXPECT_NE("-1", result);

  ASSERT_EQ(1, GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestDenyAndThenAllowDomain) {
  ASSERT_TRUE(embedded_test_server()->Start());

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
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestCreateDenyCloseNotifications) {
  ASSERT_TRUE(embedded_test_server()->Start());

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
  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  message_center::MessageCenter::Get()->RemoveNotification(
      (*notifications.rbegin())->id(),
      true);  // by_user
  ASSERT_EQ(0, GetNotificationCount());
}

// Crashes on Linux/Win. See http://crbug.com/160657.
IN_PROC_BROWSER_TEST_F(
    NotificationsTest,
    DISABLED_TestOriginPrefsNotSavedInIncognito) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Verify that allow/deny origin preferences are not saved in incognito.
  Browser* incognito = CreateIncognitoBrowser();
  ui_test_utils::NavigateToURL(incognito, GetTestPageURL());
  ASSERT_TRUE(RequestAndDenyPermission(incognito));
  CloseBrowserWindow(incognito);

  incognito = CreateIncognitoBrowser();
  ui_test_utils::NavigateToURL(incognito, GetTestPageURL());
  ASSERT_TRUE(RequestAndAcceptPermission(incognito));
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

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestCloseTabWithPermissionRequestUI) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Test that user can close tab when bubble present.
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());
  EXPECT_TRUE(RequestPermissionAndWait(browser()));
  content::WebContentsDestroyedWatcher destroyed_watcher(
      browser()->tab_strip_model()->GetWebContentsAt(0));
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabStripModel::CLOSE_NONE);
  destroyed_watcher.Wait();
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
  ASSERT_TRUE(embedded_test_server()->Start());

  // Test crashing renderer does not close or crash notification.
  AllowAllOrigins();
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());
  CreateSimpleNotification(browser(), true);
  ASSERT_EQ(1, GetNotificationCount());
  CrashTab(browser(), 0);
  ASSERT_EQ(1, GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestNotificationReplacement) {
  ASSERT_TRUE(embedded_test_server()->Start());

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

  ASSERT_EQ(1, GetNotificationCount());
  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  EXPECT_EQ(base::ASCIIToUTF16("Title2"), (*notifications.rbegin())->title());
  EXPECT_EQ(base::ASCIIToUTF16("Body2"),
            (*notifications.rbegin())->message());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest,
                       TestNotificationReplacementReappearance) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Test that we can replace a notification using the tag, and that it will
  // cause the notification to reappear as a popup again.
  AllowAllOrigins();

  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());

  ASSERT_EQ(0, GetNotificationPopupCount());

  std::string result = CreateNotification(
      browser(), true, "abc.png", "Title1", "Body1", "chat");
  EXPECT_NE("-1", result);

  ASSERT_EQ(1, GetNotificationPopupCount());

  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  message_center::MessageCenter::Get()->ClickOnNotification(
      (*notifications.rbegin())->id());

#if defined(OS_CHROMEOS)
  ASSERT_EQ(0, GetNotificationPopupCount());
#else
  ASSERT_EQ(1, GetNotificationPopupCount());
#endif

  result = CreateNotification(
      browser(), true, "abc.png", "Title2", "Body2", "chat");
  EXPECT_NE("-1", result);

  ASSERT_EQ(1, GetNotificationPopupCount());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestNotificationValidIcon) {
  ASSERT_TRUE(embedded_test_server()->Start());
  AllowAllOrigins();

  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());
  ASSERT_EQ(0, GetNotificationPopupCount());

  std::string result = CreateNotification(
      browser(), true, "icon.png", "Title1", "Body1", "chat");
  EXPECT_NE("-1", result);

  message_center::NotificationList::PopupNotifications notifications =
      message_center::MessageCenter::Get()->GetPopupNotifications();
  ASSERT_EQ(1u, notifications.size());

  auto* notification = *notifications.rbegin();

  EXPECT_EQ(100, notification->icon().Width());
  EXPECT_EQ(100, notification->icon().Height());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestNotificationInvalidIcon) {
  ASSERT_TRUE(embedded_test_server()->Start());
  AllowAllOrigins();

  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());
  ASSERT_EQ(0, GetNotificationPopupCount());

  // Not supplying an icon URL.
  std::string result = CreateNotification(
      browser(), true, "", "Title1", "Body1", "chat");
  EXPECT_NE("-1", result);

  message_center::NotificationList::PopupNotifications notifications =
      message_center::MessageCenter::Get()->GetPopupNotifications();
  ASSERT_EQ(1u, notifications.size());

  auto* notification = *notifications.rbegin();
  EXPECT_TRUE(notification->icon().IsEmpty());

  // Supplying an invalid icon URL.
  result = CreateNotification(
      browser(), true, "invalid.png", "Title1", "Body1", "chat");
  EXPECT_NE("-1", result);

  notifications = message_center::MessageCenter::Get()->GetPopupNotifications();
  ASSERT_EQ(1u, notifications.size());

  notification = *notifications.rbegin();
  EXPECT_TRUE(notification->icon().IsEmpty());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestNotificationDoubleClose) {
  ASSERT_TRUE(embedded_test_server()->Start());
  AllowAllOrigins();

  ui_test_utils::NavigateToURL(
      browser(), GetTestPageURLForFile("notification-double-close.html"));
  ASSERT_EQ(0, GetNotificationPopupCount());

  std::string result = CreateNotification(
      browser(), true, "", "Title1", "Body1", "chat");
  EXPECT_NE("-1", result);

  ASSERT_EQ(1, GetNotificationCount());
  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  message_center::MessageCenter::Get()->RemoveNotification(
      (*notifications.rbegin())->id(),
      true);  // by_user

  ASSERT_EQ(0, GetNotificationCount());

  // Calling WebContents::IsCrashed() will return FALSE here, even if the WC did
  // crash. Work around this timing issue by creating another notification,
  // which requires interaction with the renderer process.
  result = CreateNotification(browser(), true, "", "Title1", "Body1", "chat");
  EXPECT_NE("-1", result);
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestShouldDisplayNormal) {
  base::test::ScopedFeatureList scoped_feature_list;
  EnableFullscreenNotifications(&scoped_feature_list);
  ASSERT_TRUE(embedded_test_server()->Start());

  // Creates a simple notification.
  AllowAllOrigins();
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());

  std::string result = CreateSimpleNotification(browser(), true);
  EXPECT_NE("-1", result);

  ASSERT_EQ(1, GetNotificationCount());
  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();

  // Because the webpage is not fullscreen, ShouldDisplayOverFullscreen will be
  // false.
  EXPECT_FALSE(
      (*notifications.rbegin())->delegate()->ShouldDisplayOverFullscreen());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestShouldDisplayFullscreen) {
#if defined(OS_MACOSX)
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;
#endif
  base::test::ScopedFeatureList scoped_feature_list;
  EnableFullscreenNotifications(&scoped_feature_list);
  ASSERT_TRUE(embedded_test_server()->Start());

  // Creates a simple notification.
  AllowAllOrigins();
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());

  std::string result = CreateSimpleNotification(browser(), true);
  EXPECT_NE("-1", result);

  // Set the page fullscreen
  browser()->exclusive_access_manager()->fullscreen_controller()->
      ToggleBrowserFullscreenMode();

  {
    FullscreenStateWaiter fs_state(browser(), true);
    fs_state.Wait();
  }

  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      browser()->window()->GetNativeWindow()));

  ASSERT_TRUE(browser()->window()->IsActive());

  ASSERT_EQ(1, GetNotificationCount());
  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();

  // Because the webpage is fullscreen, ShouldDisplayOverFullscreen will be true
  EXPECT_TRUE(
      (*notifications.rbegin())->delegate()->ShouldDisplayOverFullscreen());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestShouldDisplayFullscreenOff) {
#if defined(OS_MACOSX)
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;
#endif
  base::test::ScopedFeatureList scoped_feature_list;
  DisableFullscreenNotifications(&scoped_feature_list);
  ASSERT_TRUE(embedded_test_server()->Start());

  // Creates a simple notification.
  AllowAllOrigins();
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());

  std::string result = CreateSimpleNotification(browser(), true);
  EXPECT_NE("-1", result);

  // Set the page fullscreen
  browser()->exclusive_access_manager()->fullscreen_controller()->
      ToggleBrowserFullscreenMode();

  {
    FullscreenStateWaiter fs_state(browser(), true);
    fs_state.Wait();
  }

  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      browser()->window()->GetNativeWindow()));

  ASSERT_TRUE(browser()->window()->IsActive());

  ASSERT_EQ(1, GetNotificationCount());
  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();

  // When the experiment flag is off, then ShouldDisplayOverFullscreen should
  // return false.
  EXPECT_FALSE(
      (*notifications.rbegin())->delegate()->ShouldDisplayOverFullscreen());
}

// The Fake OSX fullscreen window doesn't like drawing a second fullscreen
// window when another is visible.
#if !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(NotificationsTest, TestShouldDisplayMultiFullscreen) {
  base::test::ScopedFeatureList scoped_feature_list;
  EnableFullscreenNotifications(&scoped_feature_list);
  ASSERT_TRUE(embedded_test_server()->Start());
  AllowAllOrigins();

  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());

  Browser* other_browser = CreateBrowser(browser()->profile());
  ui_test_utils::NavigateToURL(other_browser, GURL("about:blank"));

  std::string result = CreateSimpleNotification(browser(), true);
  EXPECT_NE("-1", result);

  // Set the notifcation page fullscreen
  browser()->exclusive_access_manager()->fullscreen_controller()->
      ToggleBrowserFullscreenMode();
  {
    FullscreenStateWaiter fs_state(browser(), true);
    fs_state.Wait();
  }

  // Set the other browser fullscreen
  other_browser->exclusive_access_manager()->fullscreen_controller()->
      ToggleBrowserFullscreenMode();
  {
    FullscreenStateWaiter fs_state(other_browser, true);
    fs_state.Wait();
  }

  ASSERT_TRUE(browser()->exclusive_access_manager()->context()->IsFullscreen());
  ASSERT_TRUE(
      other_browser->exclusive_access_manager()->context()->IsFullscreen());

  ASSERT_FALSE(browser()->window()->IsActive());
  ASSERT_TRUE(other_browser->window()->IsActive());

  ASSERT_EQ(1, GetNotificationCount());
  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  // Because the second page is the top-most fullscreen,
  // ShouldDisplayOverFullscreen will be false
  EXPECT_FALSE(
      (*notifications.rbegin())->delegate()->ShouldDisplayOverFullscreen());
}
#endif

// Verify that a notification is actually displayed when the webpage that
// creates it is fullscreen with the fullscreen notification flag turned on.
IN_PROC_BROWSER_TEST_F(NotificationsTest, TestShouldDisplayPopupNotification) {
#if defined(OS_MACOSX)
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;
#endif
  base::test::ScopedFeatureList scoped_feature_list;
  EnableFullscreenNotifications(&scoped_feature_list);
  ASSERT_TRUE(embedded_test_server()->Start());

  // Creates a simple notification.
  AllowAllOrigins();
  ui_test_utils::NavigateToURL(browser(), GetTestPageURL());

  // Set the page fullscreen
  browser()->exclusive_access_manager()->fullscreen_controller()->
      ToggleBrowserFullscreenMode();

  {
    FullscreenStateWaiter fs_state(browser(), true);
    fs_state.Wait();
  }

  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      browser()->window()->GetNativeWindow()));

  std::string result = CreateSimpleNotification(browser(), true);
  EXPECT_NE("-1", result);

  ASSERT_EQ(1, GetNotificationCount());
  message_center::NotificationList::PopupNotifications notifications =
      message_center::MessageCenter::Get()->GetPopupNotifications();
  ASSERT_EQ(1u, notifications.size());
}
