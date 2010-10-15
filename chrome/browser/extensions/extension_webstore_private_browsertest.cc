// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/extension_webstore_private_api.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/ui_test_utils.h"
#include "googleurl/src/gurl.h"
#include "net/base/mock_host_resolver.h"

using chrome::kHttpScheme;
using chrome::kStandardSchemeSeparator;

namespace {

const char kTestUrlHostname[] = "www.example.com";

}  // namespace

// A fake version of ProfileSyncService used for testing.
class FakeProfileSyncService : public ProfileSyncService {
 public:
  // The |username_after_login| parameter determines what this fake
  // ProfileSyncService will set the username to when ShowLoginDialog is called.
  FakeProfileSyncService(const std::string& initial_username,
                         const std::string& username_after_login) :
      username_(ASCIIToUTF16(initial_username)),
      username_after_login_(username_after_login),
      observer_(NULL) {}

  virtual ~FakeProfileSyncService() {
    EXPECT_TRUE(observer_ == NULL);
  }

  // Overrides of virtual methods in ProfileSyncService.
  virtual string16 GetAuthenticatedUsername() const {
    return username_;
  }
  virtual void ShowLoginDialog(gfx::NativeWindow) {
    EXPECT_TRUE(observer_ != NULL);
    username_ = ASCIIToUTF16(username_after_login_);
    observer_->OnStateChanged();
  }
  virtual bool SetupInProgress() const {
    return false;
  }
  virtual void AddObserver(ProfileSyncServiceObserver* observer) {
    EXPECT_TRUE(observer_ == NULL);
    observer_ = observer;
  }
  virtual void RemoveObserver(ProfileSyncServiceObserver* observer) {
    EXPECT_TRUE(observer == observer_);
    observer_ = NULL;
  }

 private:
  string16 username_;
  std::string username_after_login_;
  ProfileSyncServiceObserver* observer_;
};

class ExtensionWebstorePrivateBrowserTest : public ExtensionBrowserTest {
 public:
  ExtensionWebstorePrivateBrowserTest() {
    test_url_base_ = std::string() + kHttpScheme + kStandardSchemeSeparator +
                     kTestUrlHostname;
  }

  void SetUpCommandLine(CommandLine* command_line) {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kAppsGalleryURL, test_url_base_);
  }

  // This generates a regular test server url pointing to a test file at
  // |relative_path|, but replaces the hostname with kTestUrlHostname so that
  // we get the webstore private APIs injected (this happens because of the
  // command line switch we added in SetupCommandLine).
  GURL GetUrl(const std::string& relative_path) {
    GURL base_url = test_server()->GetURL(
        "files/extensions/webstore_private/" + relative_path);
    GURL::Replacements replacements;
    std::string replacement_host = std::string(kTestUrlHostname);
    replacements.SetHostStr(replacement_host);
    return base_url.ReplaceComponents(replacements);
  }

  void RunLoginTest(const std::string& relative_path,
                    const std::string& initial_login,
                    const std::string& login_result) {
    FakeProfileSyncService sync_service(initial_login, login_result);
    WebstorePrivateApi::SetTestingProfileSyncService(&sync_service);
    ExtensionTestMessageListener listener("success", false);
    GURL url = GetUrl(relative_path);
    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    WebstorePrivateApi::SetTestingProfileSyncService(NULL);
  }

 protected:
  std::string test_url_base_;
};

IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateBrowserTest, BrowserLogin) {
  host_resolver()->AddRule(kTestUrlHostname, "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  RunLoginTest("browser_login/expect_nonempty.html", "foo@bar.com", "");
  RunLoginTest("browser_login/prompt_no_preferred.html", "", "");
  RunLoginTest("browser_login/prompt_preferred.html", "", "foo@bar.com");
  RunLoginTest("browser_login/prompt_already_logged_in_error.html",
               "foo@bar.com",
               "foo@bar.com");
}
