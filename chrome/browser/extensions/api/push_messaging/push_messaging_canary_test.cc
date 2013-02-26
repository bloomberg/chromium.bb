// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/string16.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/api/push_messaging/sync_setup_helper.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/platform_app_launcher.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/mock_host_resolver.h"

namespace switches {
const char kPasswordFileForTest[] = "password-file-for-test";
const char kOverrideUserDataDir[] = "override-user-data-dir";
}

namespace extensions {

// This class provides tests specific to the push messaging
// canary test server.  These tests require network access,
// and should not be run by normal buildbots as part of the normal
// checkin process.
class PushMessagingCanaryTest : public ExtensionApiTest {
 public:
  PushMessagingCanaryTest() {
    sync_setup_helper_.reset(new SyncSetupHelper());
  }

  ~PushMessagingCanaryTest() {
  }

  virtual void SetUp() OVERRIDE {
    CommandLine* command_line = CommandLine::ForCurrentProcess();

    if (command_line->HasSwitch(switches::kPasswordFileForTest)) {
      base::FilePath password_file =
          command_line->GetSwitchValuePath(switches::kPasswordFileForTest);
      ASSERT_TRUE(sync_setup_helper_->ReadPasswordFile(password_file));
    }

    // The test framework overrides any command line user-data-dir
    // argument with a /tmp/.org.chromium.Chromium.XXXXXX directory.
    // That happens in the ChromeTestLauncherDelegate, and affects
    // all unit tests (no opt out available).  It intentionally erases
    // any --user-data-dir switch if present and appends a new one.
    // Re-override the default data dir for our test so we can persist
    // the profile for this particular test so we can persist the max
    // invalidation version between runs.
    base::FilePath user_data_dir =
        command_line->GetSwitchValuePath(switches::kUserDataDir);
    base::FilePath override_user_data_dir =
        command_line->GetSwitchValuePath(switches::kOverrideUserDataDir);

    if (!override_user_data_dir.empty() &&
        override_user_data_dir != user_data_dir) {
      CommandLine::SwitchMap switches =
          CommandLine::ForCurrentProcess()->GetSwitches();
      command_line->AppendSwitchPath(switches::kUserDataDir,
                                     base::FilePath(override_user_data_dir));
      LOG(INFO) << "command line file override switch is "
                << override_user_data_dir.AsUTF8Unsafe();
    }

    ExtensionApiTest::SetUp();
  }

  void InitializeSync(Profile* profile) {
    ASSERT_TRUE(sync_setup_helper_->InitializeSync(profile));
  }

  // InProcessBrowserTest override. Destroys the sync client and sync
  // profile created by the test.  We must clean up ProfileSyncServiceHarness
  // now before the profile is cleaned up.
  virtual void CleanUpOnMainThread() OVERRIDE {
    sync_setup_helper_.reset();
  }

  void StartRunningTest() {
    // Get the credentials from the setup helper.
    const std::string& client_id = sync_setup_helper_->client_id();
    const std::string& client_secret = sync_setup_helper_->client_secret();
    const std::string& refresh_token = sync_setup_helper_->refresh_token();

    // Construct a JS string to pass in the parameters and start the test.
    std::string script_string = StringPrintf(
        "startTestWithCredentials('%s', '%s', '%s');",
        client_id.c_str(), client_secret.c_str(), refresh_token.c_str());
    string16 script16 = UTF8ToUTF16(script_string);

    browser()->tab_strip_model()->GetActiveWebContents()->GetRenderViewHost()->
        ExecuteJavascriptInWebFrame(string16(), script16);
  }

 protected:
  // Override InProcessBrowserTest. Change behavior of the default host
  // resolver to avoid DNS lookup errors, so we can make network calls.
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    // The resolver object lifetime is managed by sync_test_setup, not here.
    EnableDNSLookupForThisTest(
        new net::RuleBasedHostResolverProc(host_resolver()));
  }

  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE {
    DisableDNSLookupForThisTest();
  }


  // Change behavior of the default host resolver to allow DNS lookup
  // to proceed instead of being blocked by the test infrastructure.
  void EnableDNSLookupForThisTest(
      net::RuleBasedHostResolverProc* host_resolver) {
    // mock_host_resolver_override_ takes ownership of the resolver.
    scoped_refptr<net::RuleBasedHostResolverProc> resolver =
        new net::RuleBasedHostResolverProc(host_resolver);
    resolver->AllowDirectLookup("*.google.com");
    // On Linux, we use Chromium's NSS implementation which uses the following
    // hosts for certificate verification. Without these overrides, running the
    // integration tests on Linux causes error as we make external DNS lookups.
    resolver->AllowDirectLookup("*.thawte.com");
    resolver->AllowDirectLookup("*.geotrust.com");
    resolver->AllowDirectLookup("*.gstatic.com");
    resolver->AllowDirectLookup("*.googleapis.com");
    mock_host_resolver_override_.reset(
        new net::ScopedDefaultHostResolverProc(resolver));
  }

  // We need to reset the DNS lookup when we finish, or the test will fail.
  void DisableDNSLookupForThisTest() {
    mock_host_resolver_override_.reset();
  }

 private:
  scoped_ptr<SyncSetupHelper> sync_setup_helper_;

  // This test needs to make live DNS requests for access to
  // GAIA and sync server URLs under google.com. We use a scoped version
  // to override the default resolver while the test is active.
  scoped_ptr<net::ScopedDefaultHostResolverProc> mock_host_resolver_override_;
};

// Test that a push can make a round trip through the servers.
// This test is disabled to keep it from running on trybots since
// it requires network access, and it is not a good idea to run
// this test as part of a checkin or nightly test.
IN_PROC_BROWSER_TEST_F(PushMessagingCanaryTest, DISABLED_ReceivesPush) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());
  ExtensionTestMessageListener ready("ready", true);

  // Turn on sync so we can receive notifications over sync.
  InitializeSync(browser()->profile());

  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("push_messaging_canary"));
  ASSERT_TRUE(extension);

  DVLOG(1) << "extension dir is " << test_data_dir_.value();

  ui_test_utils::NavigateToURL(
      browser(), extension->GetResourceURL("push_messaging_canary.html"));

  EXPECT_TRUE(ready.WaitUntilSatisfied());

  DVLOG(1) << "push_canary app done loading";

  StartRunningTest();

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  LOG(INFO) << "Exiting Push Messaging Canary test";
}

}  // namespace extensions
