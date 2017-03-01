// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/desktop_notification_profile_util.h"
#include "chrome/browser/notifications/message_center_display_service.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/notifications/web_notification_delegate.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/permissions/permission_result.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/filename_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission_status.mojom.h"

#if BUILDFLAG(ENABLE_BACKGROUND)
#include "chrome/browser/lifetime/keep_alive_registry.h"
#include "chrome/browser/lifetime/keep_alive_types.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#endif

// -----------------------------------------------------------------------------

// Dimensions of the icon.png resource in the notification test data directory.
const int kIconWidth = 100;
const int kIconHeight = 100;

const int kNotificationVibrationPattern[] = { 100, 200, 300 };
const double kNotificationTimestamp = 621046800000.;

class PlatformNotificationServiceBrowserTest : public InProcessBrowserTest {
 public:
  PlatformNotificationServiceBrowserTest();
  ~PlatformNotificationServiceBrowserTest() override {}

  // InProcessBrowserTest overrides.
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUp() override;
  void SetUpOnMainThread() override;
  void TearDown() override;

 protected:
  // Returns the Platform Notification Service these unit tests are for.
  PlatformNotificationServiceImpl* service() const {
    return PlatformNotificationServiceImpl::GetInstance();
  }

  // Grants permission to display Web Notifications for origin of the test
  // page that's being used in this browser test.
  void GrantNotificationPermissionForTest() const;

  bool RequestAndAcceptPermission();
  bool RequestAndDenyPermission();

  void EnableFullscreenNotifications();
  void DisableFullscreenNotifications();

  // Returns the UI Manager on which notifications will be displayed.
  StubNotificationUIManager* ui_manager() const { return ui_manager_.get(); }

  const base::FilePath& server_root() const { return server_root_; }

  // Navigates the browser to the test page indicated by |path|.
  void NavigateToTestPage(const std::string& path) const;

  // Executes |script| and stores the result as a string in |result|. A boolean
  // will be returned, indicating whether the script was executed successfully.
  bool RunScript(const std::string& script, std::string* result) const;

  net::HostPortPair ServerHostPort() const;
  GURL TestPageUrl() const;

 private:
  std::string RequestAndRespondToPermission(
      PermissionRequestManager::AutoResponseType bubble_response);

  content::WebContents* GetActiveWebContents(Browser* browser) {
    return browser->tab_strip_model()->GetActiveWebContents();
  }

  const base::FilePath server_root_;
  const std::string test_page_url_;
  std::unique_ptr<StubNotificationUIManager> ui_manager_;
  std::unique_ptr<MessageCenterDisplayService> display_service_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::test::ScopedFeatureList feature_list_;
};

// -----------------------------------------------------------------------------

namespace {
const char kTestFileName[] = "notifications/platform_notification_service.html";
}

PlatformNotificationServiceBrowserTest::PlatformNotificationServiceBrowserTest()
    : server_root_(FILE_PATH_LITERAL("chrome/test/data")),
      // The test server has a base directory that doesn't exist in the
      // filesystem.
      test_page_url_(std::string("/") + kTestFileName) {}

void PlatformNotificationServiceBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitch(switches::kEnableExperimentalWebPlatformFeatures);

  InProcessBrowserTest::SetUpCommandLine(command_line);
}

void PlatformNotificationServiceBrowserTest::SetUp() {
  ui_manager_.reset(new StubNotificationUIManager);
  https_server_.reset(
      new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
  https_server_->ServeFilesFromSourceDirectory(server_root_);
  ASSERT_TRUE(https_server_->Start());
  InProcessBrowserTest::SetUp();
}

void PlatformNotificationServiceBrowserTest::SetUpOnMainThread() {
  NavigateToTestPage(test_page_url_);
  display_service_.reset(
      new MessageCenterDisplayService(browser()->profile(), ui_manager_.get()));
  service()->SetNotificationDisplayServiceForTesting(display_service_.get());
  InProcessBrowserTest::SetUpOnMainThread();
}

void PlatformNotificationServiceBrowserTest::TearDown() {
  service()->SetNotificationDisplayServiceForTesting(nullptr);
}

void PlatformNotificationServiceBrowserTest::
    GrantNotificationPermissionForTest() const {
  GURL origin = TestPageUrl().GetOrigin();

  DesktopNotificationProfileUtil::GrantPermission(browser()->profile(), origin);
  ASSERT_EQ(CONTENT_SETTING_ALLOW,
            PermissionManager::Get(browser()->profile())
                ->GetPermissionStatus(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                      origin, origin)
                .content_setting);
}

void PlatformNotificationServiceBrowserTest::NavigateToTestPage(
    const std::string& path) const {
  ui_test_utils::NavigateToURL(browser(), https_server_->GetURL(path));
}

bool PlatformNotificationServiceBrowserTest::RunScript(
    const std::string& script, std::string* result) const {
  return content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
      script,
      result);
}

net::HostPortPair PlatformNotificationServiceBrowserTest::ServerHostPort()
    const {
  return https_server_->host_port_pair();
}

GURL PlatformNotificationServiceBrowserTest::TestPageUrl() const {
  return https_server_->GetURL(test_page_url_);
}

std::string
PlatformNotificationServiceBrowserTest::RequestAndRespondToPermission(
    PermissionRequestManager::AutoResponseType bubble_response) {
  std::string result;
  content::WebContents* web_contents = GetActiveWebContents(browser());
  PermissionRequestManager::FromWebContents(web_contents)
      ->set_auto_response_for_test(bubble_response);
  EXPECT_TRUE(RunScript("RequestPermission();", &result));
  return result;
}

bool PlatformNotificationServiceBrowserTest::RequestAndAcceptPermission() {
  std::string result =
      RequestAndRespondToPermission(PermissionRequestManager::ACCEPT_ALL);
  return "granted" == result;
}

bool PlatformNotificationServiceBrowserTest::RequestAndDenyPermission() {
  std::string result =
      RequestAndRespondToPermission(PermissionRequestManager::DENY_ALL);
  return "denied" == result;
}

void PlatformNotificationServiceBrowserTest::EnableFullscreenNotifications() {
  feature_list_.InitWithFeatures({
    features::kPreferHtmlOverPlugins,
    features::kAllowFullscreenWebNotificationsFeature}, {});
}

void PlatformNotificationServiceBrowserTest::DisableFullscreenNotifications() {
  feature_list_.InitWithFeatures(
      {features::kPreferHtmlOverPlugins},
      {features::kAllowFullscreenWebNotificationsFeature});
}

// -----------------------------------------------------------------------------

// TODO(peter): Move PlatformNotificationService-related tests over from
// notification_browsertest.cc to this file.

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       DisplayPersistentNotificationWithoutPermission) {
  RequestAndDenyPermission();

  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotification()", &script_result));
  EXPECT_EQ(
      "TypeError: No notification permission has been granted for this origin.",
      script_result);

  ASSERT_EQ(0u, ui_manager()->GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       DisplayPersistentNotificationWithPermission) {
  RequestAndAcceptPermission();

  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotification('action_none')",
       &script_result));
  EXPECT_EQ("ok", script_result);

  ASSERT_EQ(1u, ui_manager()->GetNotificationCount());

#if BUILDFLAG(ENABLE_BACKGROUND)
  ASSERT_FALSE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::PENDING_NOTIFICATION_CLICK_EVENT));
#endif

  const Notification& notification = ui_manager()->GetNotificationAt(0);
  notification.delegate()->Click();

#if BUILDFLAG(ENABLE_BACKGROUND)
  ASSERT_TRUE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::PENDING_NOTIFICATION_CLICK_EVENT));
#endif

  ASSERT_TRUE(RunScript("GetMessageFromWorker()", &script_result));
  EXPECT_EQ("action_none", script_result);

#if BUILDFLAG(ENABLE_BACKGROUND)
  ASSERT_FALSE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::PENDING_NOTIFICATION_CLICK_EVENT));
#endif

  ASSERT_EQ(1u, ui_manager()->GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       WebNotificationOptionsReflection) {
  ASSERT_NO_FATAL_FAILURE(GrantNotificationPermissionForTest());

  // First, test the default values.

  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotification('Some title', {})",
                        &script_result));
  EXPECT_EQ("ok", script_result);

  ASSERT_EQ(1u, ui_manager()->GetNotificationCount());

  // We don't use or check the notification's direction and language.
  const Notification& default_notification = ui_manager()->GetNotificationAt(0);
  EXPECT_EQ("Some title", base::UTF16ToUTF8(default_notification.title()));
  EXPECT_EQ("", base::UTF16ToUTF8(default_notification.message()));
  EXPECT_EQ("", default_notification.tag());
  EXPECT_TRUE(default_notification.image().IsEmpty());
  EXPECT_TRUE(default_notification.icon().IsEmpty());
  EXPECT_TRUE(default_notification.small_image().IsEmpty());
  EXPECT_FALSE(default_notification.renotify());
  EXPECT_FALSE(default_notification.silent());
  EXPECT_FALSE(default_notification.never_timeout());
  EXPECT_EQ(0u, default_notification.buttons().size());

  // Verifies that the notification's default timestamp is set in the last 30
  // seconds. This number has no significance, but it needs to be significantly
  // high to avoid flakiness in the test.
  EXPECT_NEAR(default_notification.timestamp().ToJsTime(),
              base::Time::Now().ToJsTime(), 30 * 1000);

  // Now, test the non-default values.

  ASSERT_TRUE(RunScript("DisplayPersistentAllOptionsNotification()",
                        &script_result));
  EXPECT_EQ("ok", script_result);

  ASSERT_EQ(2u, ui_manager()->GetNotificationCount());

  // We don't use or check the notification's direction and language.
  const Notification& all_options_notification =
      ui_manager()->GetNotificationAt(1);
  EXPECT_EQ("Title", base::UTF16ToUTF8(all_options_notification.title()));
  EXPECT_EQ("Contents", base::UTF16ToUTF8(all_options_notification.message()));
  EXPECT_EQ("replace-id", all_options_notification.tag());
  EXPECT_FALSE(all_options_notification.image().IsEmpty());
  EXPECT_EQ(kIconWidth, all_options_notification.image().Width());
  EXPECT_EQ(kIconHeight, all_options_notification.image().Height());
  EXPECT_FALSE(all_options_notification.icon().IsEmpty());
  EXPECT_EQ(kIconWidth, all_options_notification.icon().Width());
  EXPECT_EQ(kIconHeight, all_options_notification.icon().Height());
  EXPECT_TRUE(all_options_notification.small_image().IsEmpty());
  EXPECT_TRUE(all_options_notification.renotify());
  EXPECT_TRUE(all_options_notification.silent());
  EXPECT_TRUE(all_options_notification.never_timeout());
  EXPECT_DOUBLE_EQ(kNotificationTimestamp,
                   all_options_notification.timestamp().ToJsTime());
  EXPECT_EQ(1u, all_options_notification.buttons().size());
  EXPECT_EQ("actionTitle",
            base::UTF16ToUTF8(all_options_notification.buttons()[0].title));
  EXPECT_FALSE(all_options_notification.buttons()[0].icon.IsEmpty());
  EXPECT_EQ(kIconWidth, all_options_notification.buttons()[0].icon.Width());
  EXPECT_EQ(kIconHeight, all_options_notification.buttons()[0].icon.Height());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       WebNotificationSiteSettingsButton) {
  ASSERT_NO_FATAL_FAILURE(GrantNotificationPermissionForTest());

  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotification('Some title', {})",
                        &script_result));
  EXPECT_EQ("ok", script_result);

  ASSERT_EQ(1u, ui_manager()->GetNotificationCount());

  const Notification& notification = ui_manager()->GetNotificationAt(0);
  const std::vector<message_center::ButtonInfo>& buttons =
      notification.buttons();
  EXPECT_EQ(0u, buttons.size());

  notification.delegate()->SettingsClick();
  ASSERT_EQ(1u, ui_manager()->GetNotificationCount());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));

  std::string url = web_contents->GetLastCommittedURL().spec();
  if (base::FeatureList::IsEnabled(features::kMaterialDesignSettings))
    ASSERT_EQ("chrome://settings/content/notifications", url);
  else
    ASSERT_EQ("chrome://settings/contentExceptions#notifications", url);
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       WebNotificationOptionsVibrationPattern) {
  ASSERT_NO_FATAL_FAILURE(GrantNotificationPermissionForTest());

  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotificationVibrate()",
                        &script_result));
  EXPECT_EQ("ok", script_result);

  ASSERT_EQ(1u, ui_manager()->GetNotificationCount());

  const Notification& notification = ui_manager()->GetNotificationAt(0);
  EXPECT_EQ("Title", base::UTF16ToUTF8(notification.title()));
  EXPECT_EQ("Contents", base::UTF16ToUTF8(notification.message()));

  EXPECT_THAT(notification.vibration_pattern(),
      testing::ElementsAreArray(kNotificationVibrationPattern));
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       CloseDisplayedPersistentNotification) {
  ASSERT_NO_FATAL_FAILURE(GrantNotificationPermissionForTest());

  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotification('action_close')",
                        &script_result));
  EXPECT_EQ("ok", script_result);

  ASSERT_EQ(1u, ui_manager()->GetNotificationCount());

  const Notification& notification = ui_manager()->GetNotificationAt(0);
  notification.delegate()->Click();

  ASSERT_TRUE(RunScript("GetMessageFromWorker()", &script_result));
  EXPECT_EQ("action_close", script_result);

  ASSERT_EQ(0u, ui_manager()->GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       UserClosesPersistentNotification) {
  ASSERT_NO_FATAL_FAILURE(GrantNotificationPermissionForTest());

  std::string script_result;
  ASSERT_TRUE(
      RunScript("DisplayPersistentNotification('close_test')", &script_result));
  EXPECT_EQ("ok", script_result);

  ASSERT_EQ(1u, ui_manager()->GetNotificationCount());
  const Notification& notification = ui_manager()->GetNotificationAt(0);
  notification.delegate()->Close(true /* by_user */);

  ASSERT_TRUE(RunScript("GetMessageFromWorker()", &script_result));
  EXPECT_EQ("closing notification: close_test", script_result);
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       TestDisplayOriginContextMessage) {
  RequestAndAcceptPermission();

  // Creates a simple notification.
  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotification()", &script_result));

  net::HostPortPair host_port = ServerHostPort();

  const Notification& notification = ui_manager()->GetNotificationAt(0);

  EXPECT_TRUE(notification.context_message().empty());
  EXPECT_EQ("https://" + host_port.ToString() + "/",
            notification.origin_url().spec());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       PersistentNotificationServiceWorkerScope) {
  RequestAndAcceptPermission();

  // Creates a simple notification.
  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotification()", &script_result));

  const Notification& notification = ui_manager()->GetNotificationAt(0);

  EXPECT_EQ(TestPageUrl().spec(), notification.service_worker_scope().spec());
}

// TODO(felt): This DCHECKs when bubbles are enabled, when the file_url is
// persisted. crbug.com/502057
IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       DISABLED_CheckFilePermissionNotGranted) {
  // This case should succeed because a normal page URL is used.
  std::string script_result;

  PermissionManager* permission_manager =
      PermissionManager::Get(browser()->profile());

  EXPECT_EQ(CONTENT_SETTING_ASK,
            permission_manager
                ->GetPermissionStatus(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                      TestPageUrl(), TestPageUrl())
                .content_setting);

  RequestAndAcceptPermission();
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            permission_manager
                ->GetPermissionStatus(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                      TestPageUrl(), TestPageUrl())
                .content_setting);

  // This case should fail because a file URL is used.
  base::FilePath dir_source_root;
  EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &dir_source_root));
  base::FilePath full_file_path =
      dir_source_root.Append(server_root()).AppendASCII(kTestFileName);
  GURL file_url(net::FilePathToFileURL(full_file_path));

  ui_test_utils::NavigateToURL(browser(), file_url);

  EXPECT_EQ(CONTENT_SETTING_ASK,
            permission_manager
                ->GetPermissionStatus(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                      file_url, file_url)
                .content_setting);

  RequestAndAcceptPermission();
  EXPECT_EQ(CONTENT_SETTING_ASK,
            permission_manager
                ->GetPermissionStatus(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                      file_url, file_url)
                .content_setting)
      << "If this test fails, you may have fixed a bug preventing file origins "
      << "from sending their origin from Blink; if so you need to update the "
      << "display function for notification origins to show the file path.";
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       DataUrlAsNotificationImage) {
  ASSERT_NO_FATAL_FAILURE(GrantNotificationPermissionForTest());

  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotificationDataUrlImage()",
                        &script_result));
  EXPECT_EQ("ok", script_result);

  ASSERT_EQ(1u, ui_manager()->GetNotificationCount());

  const Notification& notification = ui_manager()->GetNotificationAt(0);
  EXPECT_FALSE(notification.icon().IsEmpty());

  EXPECT_EQ("Data URL Title", base::UTF16ToUTF8(notification.title()));
  EXPECT_EQ(kIconWidth, notification.icon().Width());
  EXPECT_EQ(kIconHeight, notification.icon().Height());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       BlobAsNotificationImage) {
  ASSERT_NO_FATAL_FAILURE(GrantNotificationPermissionForTest());

  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotificationBlobImage()",
                        &script_result));
  EXPECT_EQ("ok", script_result);

  ASSERT_EQ(1u, ui_manager()->GetNotificationCount());

  const Notification& notification = ui_manager()->GetNotificationAt(0);
  EXPECT_FALSE(notification.icon().IsEmpty());

  EXPECT_EQ("Blob Title", base::UTF16ToUTF8(notification.title()));
  EXPECT_EQ(kIconWidth, notification.icon().Width());
  EXPECT_EQ(kIconHeight, notification.icon().Height());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       DisplayPersistentNotificationWithActionButtons) {
  ASSERT_NO_FATAL_FAILURE(GrantNotificationPermissionForTest());

  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotificationWithActionButtons()",
                        &script_result));
  EXPECT_EQ("ok", script_result);
  ASSERT_EQ(1u, ui_manager()->GetNotificationCount());

  const Notification& notification = ui_manager()->GetNotificationAt(0);
  ASSERT_EQ(2u, notification.buttons().size());
  EXPECT_EQ("actionTitle1", base::UTF16ToUTF8(notification.buttons()[0].title));
  EXPECT_EQ("actionTitle2", base::UTF16ToUTF8(notification.buttons()[1].title));

  notification.delegate()->ButtonClick(0);
  ASSERT_TRUE(RunScript("GetMessageFromWorker()", &script_result));
  EXPECT_EQ("action_button_click actionId1", script_result);

  notification.delegate()->ButtonClick(1);
  ASSERT_TRUE(RunScript("GetMessageFromWorker()", &script_result));
  EXPECT_EQ("action_button_click actionId2", script_result);
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       DisplayPersistentNotificationWithReplyButton) {
  ASSERT_NO_FATAL_FAILURE(GrantNotificationPermissionForTest());

  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotificationWithReplyButton()",
                        &script_result));
  EXPECT_EQ("ok", script_result);
  ASSERT_EQ(1u, ui_manager()->GetNotificationCount());

  const Notification& notification = ui_manager()->GetNotificationAt(0);
  ASSERT_EQ(1u, notification.buttons().size());
  EXPECT_EQ("actionTitle1", base::UTF16ToUTF8(notification.buttons()[0].title));

  notification.delegate()->ButtonClickWithReply(0, base::ASCIIToUTF16("hello"));
  ASSERT_TRUE(RunScript("GetMessageFromWorker()", &script_result));
  EXPECT_EQ("action_button_click actionId1 hello", script_result);
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       GetDisplayedNotifications) {
  RequestAndAcceptPermission();

  std::string script_result;
  std::string script_message;
  ASSERT_TRUE(RunScript("DisplayNonPersistentNotification('NonPersistent')",
                        &script_result));
  EXPECT_EQ("ok", script_result);
  ASSERT_TRUE(RunScript("DisplayPersistentNotification('PersistentI')",
                        &script_result));
  EXPECT_EQ("ok", script_result);
  ASSERT_TRUE(RunScript("DisplayPersistentNotification('PersistentII')",
                        &script_result));
  EXPECT_EQ("ok", script_result);

  // Only the persistent ones should show.
  ASSERT_TRUE(RunScript("GetDisplayedNotifications()", &script_result));
  EXPECT_EQ("ok", script_result);

  ASSERT_TRUE(RunScript("GetMessageFromWorker()", &script_message));

  std::vector<std::string> notifications = base::SplitString(
      script_message, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(2u, notifications.size());

  // Now remove one of the notifications straight from the ui manager
  // without going through the database.
  const Notification& notification = ui_manager()->GetNotificationAt(1);

  // p: is the prefix for persistent notifications. See
  //  content/browser/notifications/notification_id_generator.{h,cc} for details
  ASSERT_TRUE(
      base::StartsWith(notification.id(), "p:", base::CompareCase::SENSITIVE));
  ASSERT_TRUE(ui_manager()->SilentDismissById(
      notification.delegate_id(),
      NotificationUIManager::GetProfileID(browser()->profile())));
  ASSERT_TRUE(RunScript("GetDisplayedNotifications()", &script_result));
  EXPECT_EQ("ok", script_result);

  ASSERT_TRUE(RunScript("GetMessageFromWorker()", &script_message));
  notifications = base::SplitString(script_message, ",", base::KEEP_WHITESPACE,
                                    base::SPLIT_WANT_ALL);
  ASSERT_EQ(1u, notifications.size());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       TestShouldDisplayNormal) {
  ASSERT_NO_FATAL_FAILURE(GrantNotificationPermissionForTest());
  EnableFullscreenNotifications();

  std::string script_result;
  ASSERT_TRUE(RunScript(
      "DisplayPersistentNotification('display_normal')", &script_result));
  EXPECT_EQ("ok", script_result);

  ASSERT_EQ(1u, ui_manager()->GetNotificationCount());
  const Notification& notification = ui_manager()->GetNotificationAt(0);
  EXPECT_FALSE(notification.delegate()->ShouldDisplayOverFullscreen());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       TestShouldDisplayFullscreen) {
#if defined(OS_MACOSX)
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;
#endif
  ASSERT_NO_FATAL_FAILURE(GrantNotificationPermissionForTest());
  EnableFullscreenNotifications();

  std::string script_result;
  ASSERT_TRUE(RunScript(
      "DisplayPersistentNotification('display_normal')", &script_result));
  EXPECT_EQ("ok", script_result);

  // Set the page fullscreen
  browser()->exclusive_access_manager()->fullscreen_controller()->
      ToggleBrowserFullscreenMode();

  {
    FullscreenStateWaiter fs_state(browser(), true);
    fs_state.Wait();
  }

  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      browser()->window()->GetNativeWindow()));

  ASSERT_TRUE(browser()->window()->IsActive())
      << "Browser is active after going fullscreen";

  ASSERT_EQ(1u, ui_manager()->GetNotificationCount());
  const Notification& notification = ui_manager()->GetNotificationAt(0);
  EXPECT_TRUE(notification.delegate()->ShouldDisplayOverFullscreen());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       TestShouldDisplayFullscreenOff) {
#if defined(OS_MACOSX)
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;
#endif
  ASSERT_NO_FATAL_FAILURE(GrantNotificationPermissionForTest());
  DisableFullscreenNotifications();

  std::string script_result;
  ASSERT_TRUE(RunScript(
      "DisplayPersistentNotification('display_normal')", &script_result));
  EXPECT_EQ("ok", script_result);

  // Set the page fullscreen
  browser()->exclusive_access_manager()->fullscreen_controller()->
      ToggleBrowserFullscreenMode();

  {
    FullscreenStateWaiter fs_state(browser(), true);
    fs_state.Wait();
  }

  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      browser()->window()->GetNativeWindow()));

  ASSERT_TRUE(browser()->window()->IsActive())
      << "Browser is active after going fullscreen";

  ASSERT_EQ(1u, ui_manager()->GetNotificationCount());
  const Notification& notification = ui_manager()->GetNotificationAt(0);
  // When the experiment flag is off, then ShouldDisplayOverFullscreen should
  // return false.
  EXPECT_FALSE(notification.delegate()->ShouldDisplayOverFullscreen());
}

// The Fake OSX fullscreen window doesn't like drawing a second fullscreen
// window when another is visible.
#if !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       TestShouldDisplayMultiFullscreen) {
  ASSERT_NO_FATAL_FAILURE(GrantNotificationPermissionForTest());
  EnableFullscreenNotifications();

  Browser* other_browser = CreateBrowser(browser()->profile());
  ui_test_utils::NavigateToURL(other_browser, GURL("about:blank"));

  std::string script_result;
  ASSERT_TRUE(RunScript(
      "DisplayPersistentNotification('display_normal')", &script_result));
  EXPECT_EQ("ok", script_result);

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

  ASSERT_EQ(1u, ui_manager()->GetNotificationCount());
  const Notification& notification = ui_manager()->GetNotificationAt(0);
  EXPECT_FALSE(notification.delegate()->ShouldDisplayOverFullscreen());
}
#endif

class PlatformNotificationServiceWithoutContentImageBrowserTest
    : public PlatformNotificationServiceBrowserTest {
 public:
  // InProcessBrowserTest overrides.
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitAndDisableFeature(
        features::kNotificationContentImage);
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    PlatformNotificationServiceWithoutContentImageBrowserTest,
    KillSwitch) {
  ASSERT_NO_FATAL_FAILURE(GrantNotificationPermissionForTest());

  std::string script_result;
  ASSERT_TRUE(
      RunScript("DisplayPersistentAllOptionsNotification()", &script_result));
  EXPECT_EQ("ok", script_result);

  ASSERT_EQ(1u, ui_manager()->GetNotificationCount());
  const Notification& notification = ui_manager()->GetNotificationAt(0);

  // Since the kNotificationContentImage kill switch has disabled images, the
  // notification should be shown without an image.
  EXPECT_TRUE(notification.image().IsEmpty());
}
