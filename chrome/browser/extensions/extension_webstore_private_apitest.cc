// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_install_dialog.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_webstore_private_api.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_service.h"
#include "net/base/mock_host_resolver.h"

namespace {

class WebstoreInstallListener : public WebstoreInstaller::Delegate {
 public:
  WebstoreInstallListener()
      : received_failure_(false), received_success_(false) {}

  void OnExtensionInstallSuccess(const std::string& id) OVERRIDE;
  void OnExtensionInstallFailure(const std::string& id,
                                 const std::string& error) OVERRIDE;
  void Wait();

  bool received_failure() const { return received_failure_; }
  bool received_success() const { return received_success_; }
  const std::string& id() const { return id_; }
  const std::string& error() const { return error_; }

 private:
  bool received_failure_;
  bool received_success_;
  std::string id_;
  std::string error_;
};

void WebstoreInstallListener::OnExtensionInstallSuccess(const std::string& id) {
  received_success_ = true;
  id_ = id;
  MessageLoopForUI::current()->Quit();
}

void WebstoreInstallListener::OnExtensionInstallFailure(
    const std::string& id, const std::string& error) {
  received_failure_ = true;
  id_ = id;
  error_ = error;
  MessageLoopForUI::current()->Quit();
}

void WebstoreInstallListener::Wait() {
  if (received_success_ || received_failure_)
    return;

  ui_test_utils::RunMessageLoop();
}

}  // namespace

// A base class for tests below.
class ExtensionWebstorePrivateApiTest : public ExtensionApiTest {
 public:
  void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kAppsGalleryURL,
        "http://www.example.com");
  }

  void SetUpInProcessBrowserTestFixture() OVERRIDE {
    // Start up the test server and get us ready for calling the install
    // API functions.
    host_resolver()->AddRule("www.example.com", "127.0.0.1");
    ASSERT_TRUE(test_server()->Start());
    BeginInstallWithManifestFunction::SetIgnoreUserGestureForTests(true);
    SetExtensionInstallDialogForManifestAutoConfirmForTests(true);
    ExtensionInstallUI::DisableFailureUIForTests();
  }

 protected:
  // Returns a test server URL, but with host 'www.example.com' so it matches
  // the web store app's extent that we set up via command line flags.
  GURL GetTestServerURL(const std::string& path) {
    GURL url = test_server()->GetURL(
        std::string("files/extensions/api_test/webstore_private/") + path);

    // Replace the host with 'www.example.com' so it matches the web store
    // app's extent.
    GURL::Replacements replace_host;
    std::string host_str("www.example.com");
    replace_host.SetHostStr(host_str);

    return url.ReplaceComponents(replace_host);
  }

  // Navigates to |page| and runs the Extension API test there. Any downloads
  // of extensions will return the contents of |crx_file|.
  bool RunInstallTest(const std::string& page, const std::string& crx_file) {
    GURL crx_url = GetTestServerURL(crx_file);
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kAppsGalleryUpdateURL, crx_url.spec());

    GURL page_url = GetTestServerURL(page);
    return RunPageTest(page_url.spec());
  }

  ExtensionService* service() {
    return browser()->profile()->GetExtensionService();
  }
};

// Test cases where the user accepts the install confirmation dialog.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, InstallAccepted) {
  ASSERT_TRUE(RunInstallTest("accepted.html", "extension.crx"));
}

// Tests passing a localized name.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, InstallLocalized) {
  ASSERT_TRUE(RunInstallTest("localized.html", "localized_extension.crx"));
}

// Now test the case where the user cancels the confirmation dialog.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, InstallCancelled) {
  SetExtensionInstallDialogForManifestAutoConfirmForTests(false);
  ASSERT_TRUE(RunInstallTest("cancelled.html", "extension.crx"));
}

IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, InstallNoGesture) {
  BeginInstallFunction::SetIgnoreUserGestureForTests(false);
  ASSERT_TRUE(RunInstallTest("no_user_gesture.html", "extension.crx"));
}

IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest,
                       IncorrectManifest1) {
  WebstoreInstallListener listener;
  WebstorePrivateApi::SetWebstoreInstallerDelegateForTesting(&listener);
  ASSERT_TRUE(RunInstallTest("incorrect_manifest1.html", "extension.crx"));
  listener.Wait();
  ASSERT_TRUE(listener.received_failure());
  ASSERT_EQ("Manifest file is invalid.", listener.error());
}

IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest,
                       IncorrectManifest2) {
  WebstoreInstallListener listener;
  WebstorePrivateApi::SetWebstoreInstallerDelegateForTesting(&listener);
  ASSERT_TRUE(RunInstallTest("incorrect_manifest2.html", "extension.crx"));
  listener.Wait();
  EXPECT_TRUE(listener.received_failure());
  ASSERT_EQ("Manifest file is invalid.", listener.error());
}

// Tests that we can request an app installed bubble (instead of the default
// UI when an app is installed).
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest,
                       AppInstallBubble) {
  WebstoreInstallListener listener;
  WebstorePrivateApi::SetWebstoreInstallerDelegateForTesting(&listener);
  ASSERT_TRUE(RunInstallTest("app_install_bubble.html", "app.crx"));
  listener.Wait();
  ASSERT_TRUE(listener.received_success());
  ASSERT_EQ("iladmdjkfniedhfhcfoefgojhgaiaccc", listener.id());
}

// Tests using the iconUrl parameter to the install function.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest,
                       IconUrl) {
  ASSERT_TRUE(RunInstallTest("icon_url.html", "extension.crx"));
}
