// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_signin.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/extension_webstore_private_api.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
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
  FakeProfileSyncService()
      : setup_(false) {
  }
  virtual ~FakeProfileSyncService() {}

  // Overrides of virtual methods in ProfileSyncService.
  virtual bool HasSyncSetupCompleted() const {
    return setup_;
  }
  virtual void ChangePreferredDataTypes(const syncable::ModelTypeSet& types) {
    types_ = types;
  }
  virtual void GetPreferredDataTypes(syncable::ModelTypeSet* types) const {
    *types = types_;
  }
  virtual void SetSyncSetupCompleted() {
    setup_ = true;
  }

 private:
  bool setup_;
  syncable::ModelTypeSet types_;
};

class FakeBrowserSignin : public BrowserSignin {
 public:
  // The |username_after_login| parameter determines what this fake
  // BrowserSignin will set the username to when ShowLoginDialog is called.
  FakeBrowserSignin(bool should_succeed,
                    const std::string& initial_username,
                    const std::string& username_after_login)
      : BrowserSignin(NULL),
        should_succeed_(should_succeed),
        username_(initial_username),
        username_after_login_(username_after_login) {
  }
  virtual ~FakeBrowserSignin() {}

  virtual std::string GetSignedInUsername() const {
    return username_;
  }

  virtual void RequestSignin(TabContents* tab_contents,
                             const string16& preferred_email,
                             const string16& message,
                             SigninDelegate* delegate) {
    if (should_succeed_) {
      // Simulate valid login.
      username_ = username_after_login_;
      delegate->OnLoginSuccess();

      // Fake a token available notification.
      Profile* profile = tab_contents->profile();
      if (profile->IsOffTheRecord()) {
        profile = g_browser_process->profile_manager()->GetDefaultProfile();
      }
      TokenService* token_service = profile->GetTokenService();
      token_service->IssueAuthTokenForTest(GaiaConstants::kGaiaService,
                                           "new_token");
    } else {
      delegate->OnLoginFailure(GoogleServiceAuthError(
        GoogleServiceAuthError::REQUEST_CANCELED));
    }
  }

 private:
  bool should_succeed_;
  std::string username_;
  std::string username_after_login_;
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

  void RunLoginTestImpl(bool incognito,
                        const std::string& relative_path,
                        const std::string& initial_login,
                        bool login_succeeds,
                        const std::string& login_result) {
    // Clear the token service so previous tests don't affect things.
    TokenService* token_service = browser()->profile()->GetTokenService();
    token_service->ResetCredentialsInMemory();
    if (!initial_login.empty()) {
      // Initialize the token service with an existing token.
      token_service->IssueAuthTokenForTest(GaiaConstants::kGaiaService,
                                           "existing_token");
    }

    FakeProfileSyncService sync_service;
    FakeBrowserSignin signin(login_succeeds, initial_login, login_result);
    WebstorePrivateApi::SetTestingProfileSyncService(&sync_service);
    WebstorePrivateApi::SetTestingBrowserSignin(&signin);

    ExtensionTestMessageListener listener("success", false);
    GURL url = GetUrl(relative_path);
    if (incognito) {
      ui_test_utils::OpenURLOffTheRecord(browser()->profile(), url);
    } else {
      ui_test_utils::NavigateToURL(browser(), url);
    }
    EXPECT_TRUE(listener.WaitUntilSatisfied());

    WebstorePrivateApi::SetTestingBrowserSignin(NULL);
    WebstorePrivateApi::SetTestingProfileSyncService(NULL);
  }

  void RunLoginTest(const std::string& relative_path,
                    const std::string& initial_login,
                    bool login_succeeds,
                    const std::string& login_result) {
    RunLoginTestImpl(true,
                     relative_path,
                     initial_login,
                     login_succeeds,
                     login_result);
    RunLoginTestImpl(false,
                     relative_path,
                     initial_login,
                     login_succeeds,
                     login_result);
  }

 protected:
  std::string test_url_base_;
};

IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateBrowserTest, BrowserLogin) {
  host_resolver()->AddRule(kTestUrlHostname, "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  RunLoginTest("browser_login/expect_nonempty.html",
               "foo@bar.com", false, "");

  RunLoginTest("browser_login/prompt_no_preferred.html", "", true, "");

  RunLoginTest("browser_login/prompt_preferred.html", "", true, "foo@bar.com");

  RunLoginTest("browser_login/prompt_login_fails.html",
               "", false, "foo@bar.com");
}
