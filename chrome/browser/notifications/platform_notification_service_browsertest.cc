// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/filename_util.h"
#include "net/test/spawned_test_server/spawned_test_server.h"

// -----------------------------------------------------------------------------

// Accept or rejects the first shown confirm infobar. The infobar will be
// responsed to asynchronously, to imitate the behavior of a user.
// TODO(peter): Generalize this class, as it's commonly useful.
class InfoBarResponder : public infobars::InfoBarManager::Observer {
 public:
  InfoBarResponder(Browser* browser, bool accept);
  ~InfoBarResponder() override;

  // infobars::InfoBarManager::Observer overrides.
  void OnInfoBarAdded(infobars::InfoBar* infobar) override;

 private:
  void Respond(ConfirmInfoBarDelegate* delegate);

  InfoBarService* infobar_service_;
  bool accept_;
  bool has_observed_;
};

InfoBarResponder::InfoBarResponder(Browser* browser, bool accept)
    : infobar_service_(InfoBarService::FromWebContents(
            browser->tab_strip_model()->GetActiveWebContents())),
      accept_(accept),
      has_observed_(false) {
  infobar_service_->AddObserver(this);
}

InfoBarResponder::~InfoBarResponder() {
  infobar_service_->RemoveObserver(this);
}

void InfoBarResponder::OnInfoBarAdded(infobars::InfoBar* infobar) {
  if (has_observed_)
    return;

  has_observed_ = true;
  ConfirmInfoBarDelegate* delegate =
      infobar->delegate()->AsConfirmInfoBarDelegate();
  DCHECK(delegate);

  // Respond to the infobar asynchronously, like a person.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(
          &InfoBarResponder::Respond, base::Unretained(this), delegate));
}

void InfoBarResponder::Respond(ConfirmInfoBarDelegate* delegate) {
  if (accept_)
    delegate->Accept();
  else
    delegate->Cancel();
}

// -----------------------------------------------------------------------------

class PlatformNotificationServiceBrowserTest : public InProcessBrowserTest {
 public:
  PlatformNotificationServiceBrowserTest();
  ~PlatformNotificationServiceBrowserTest() override {}

  // InProcessBrowserTest overrides.
  void SetUp() override;
  void SetUpOnMainThread() override;
  void TearDown() override;

 protected:
  // Returns the Platform Notification Service these unit tests are for.
  PlatformNotificationServiceImpl* service() const {
    return PlatformNotificationServiceImpl::GetInstance();
  }

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
  const base::FilePath server_root_;
  const std::string test_page_url_;
  scoped_ptr<StubNotificationUIManager> ui_manager_;
  scoped_ptr<net::SpawnedTestServer> https_server_;
};

// -----------------------------------------------------------------------------

namespace {
const char kTestFileName[] = "notifications/platform_notification_service.html";
}

PlatformNotificationServiceBrowserTest::PlatformNotificationServiceBrowserTest()
    : server_root_(FILE_PATH_LITERAL("chrome/test/data")),
      // The test server has a base directory that doesn't exist in the
      // filesystem.
      test_page_url_(std::string("files/") + kTestFileName) {
}

void PlatformNotificationServiceBrowserTest::SetUp() {
  ui_manager_.reset(new StubNotificationUIManager);
  https_server_.reset(new net::SpawnedTestServer(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::BaseTestServer::SSLOptions(net::BaseTestServer::SSLOptions::CERT_OK),
      server_root_));
  ASSERT_TRUE(https_server_->Start());

  service()->SetNotificationUIManagerForTesting(ui_manager_.get());

  InProcessBrowserTest::SetUp();
}

void PlatformNotificationServiceBrowserTest::SetUpOnMainThread() {
  NavigateToTestPage(test_page_url_);

  InProcessBrowserTest::SetUpOnMainThread();
}

void PlatformNotificationServiceBrowserTest::TearDown() {
  service()->SetNotificationUIManagerForTesting(nullptr);
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

// -----------------------------------------------------------------------------

// TODO(peter): Move PlatformNotificationService-related tests over from
// notification_browsertest.cc to this file.

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       DisplayPersistentNotificationWithoutPermission) {
  std::string script_result;

  InfoBarResponder accepting_responder(browser(), false);
  ASSERT_TRUE(RunScript("RequestPermission()", &script_result));
  EXPECT_EQ("denied", script_result);

  ASSERT_TRUE(RunScript("DisplayPersistentNotification()", &script_result));
  EXPECT_EQ(
      "TypeError: No notification permission has been granted for this origin.",
      script_result);

  ASSERT_EQ(0u, ui_manager()->GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       DisplayPersistentNotificationWithPermission) {
  std::string script_result;

  InfoBarResponder accepting_responder(browser(), true);
  ASSERT_TRUE(RunScript("RequestPermission()", &script_result));
  EXPECT_EQ("granted", script_result);

  ASSERT_TRUE(RunScript("DisplayPersistentNotification('action_none')",
       &script_result));
  EXPECT_EQ("ok", script_result);

  ASSERT_EQ(1u, ui_manager()->GetNotificationCount());

  const Notification& notification = ui_manager()->GetNotificationAt(0);
  notification.delegate()->Click();

  ASSERT_TRUE(RunScript("GetMessageFromWorker()", &script_result));
  EXPECT_EQ("action_none", script_result);

  ASSERT_EQ(1u, ui_manager()->GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       WebNotificationOptionsReflection) {
  std::string script_result;

  // TODO(peter): It doesn't add much value if we use the InfoBarResponder for
  // each test. Rather, we should just toggle the content setting.
  InfoBarResponder accepting_responder(browser(), true);
  ASSERT_TRUE(RunScript("RequestPermission()", &script_result));
  EXPECT_EQ("granted", script_result);

  ASSERT_TRUE(RunScript("DisplayPersistentAllOptionsNotification()",
                        &script_result));
  EXPECT_EQ("ok", script_result);

  ASSERT_EQ(1u, ui_manager()->GetNotificationCount());

  // We don't use or check the notification's direction and language.
  const Notification& notification = ui_manager()->GetNotificationAt(0);
  EXPECT_EQ("Title", base::UTF16ToUTF8(notification.title()));
  EXPECT_EQ("Contents", base::UTF16ToUTF8(notification.message()));
  EXPECT_EQ("replace-id", notification.tag());
  EXPECT_FALSE(notification.icon().IsEmpty());
  EXPECT_TRUE(notification.silent());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       CloseDisplayedPersistentNotification) {
  std::string script_result;

  InfoBarResponder accepting_responder(browser(), true);
  ASSERT_TRUE(RunScript("RequestPermission()", &script_result));
  EXPECT_EQ("granted", script_result);

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
                       TestDisplayOriginContextMessage) {
  std::string script_result;

  // Creates a simple notification.
  InfoBarResponder accepting_responder(browser(), true);
  ASSERT_TRUE(RunScript("RequestPermission()", &script_result));
  ASSERT_EQ("granted", script_result);
  ASSERT_TRUE(RunScript("DisplayPersistentNotification()", &script_result));

  net::HostPortPair host_port = ServerHostPort();

  const Notification& notification = ui_manager()->GetNotificationAt(0);

  EXPECT_EQ(base::UTF8ToUTF16(host_port.ToString()),
            notification.context_message());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       CheckFilePermissionNotGranted) {
  // TODO(dewittj): This test verifies that a bug exists in Chrome; the test
  // will fail if the bug is fixed.  The
  // |PlatformNotificationServiceImpl::WebOriginDisplayName| function needs
  // to be updated to properly display file:// URL origins.
  // See crbug.com/402191.
  std::string script_result;

  InfoBarResponder accepting_responder_web(browser(), true);

  DesktopNotificationService* notification_service =
      DesktopNotificationServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(notification_service);
  message_center::NotifierId web_notifier(TestPageUrl());
  EXPECT_FALSE(notification_service->IsNotifierEnabled(web_notifier));
  ASSERT_TRUE(RunScript("RequestPermission()", &script_result));
  EXPECT_EQ("granted", script_result);

  EXPECT_TRUE(notification_service->IsNotifierEnabled(web_notifier));

  base::FilePath dir_source_root;
  EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &dir_source_root));
  base::FilePath full_file_path =
      dir_source_root.Append(server_root()).AppendASCII(kTestFileName);
  GURL file_url(net::FilePathToFileURL(full_file_path));
  ui_test_utils::NavigateToURL(browser(), file_url);

  message_center::NotifierId file_notifier(file_url);
  EXPECT_FALSE(notification_service->IsNotifierEnabled(file_notifier));

  InfoBarResponder accepting_responder_file(browser(), true);
  ASSERT_TRUE(RunScript("RequestPermission()", &script_result));
  EXPECT_EQ("granted", script_result);

  EXPECT_FALSE(notification_service->IsNotifierEnabled(file_notifier))
      << "If this test fails, you may have fixed a bug preventing file origins "
      << "from sending their origin from Blink; if so you need to update the "
      << "display function for notification origins to show the file path.";
}
