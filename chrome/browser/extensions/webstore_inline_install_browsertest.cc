// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_install_dialog.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/webstore_inline_installer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"
#include "net/base/host_port_pair.h"
#include "net/base/mock_host_resolver.h"

using content::WebContents;

const char kWebstoreDomain[] = "cws.com";
const char kAppDomain[] = "app.com";
const char kNonAppDomain[] = "nonapp.com";

class WebstoreInlineInstallTest : public InProcessBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    EnableDOMAutomation();

    // We start the test server now instead of in
    // SetUpInProcessBrowserTestFixture so that we can get its port number.
    ASSERT_TRUE(test_server()->Start());

    InProcessBrowserTest::SetUpCommandLine(command_line);

    net::HostPortPair host_port = test_server()->host_port_pair();
    test_gallery_url_ = base::StringPrintf(
        "http://%s:%d/files/extensions/api_test/webstore_inline_install",
        kWebstoreDomain, host_port.port());
    command_line->AppendSwitchASCII(
        switches::kAppsGalleryURL, test_gallery_url_);

    GURL crx_url = GenerateTestServerUrl(kWebstoreDomain, "extension.crx");
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kAppsGalleryUpdateURL, crx_url.spec());

    // Allow tests to call window.gc(), so that we can check that callback
    // functions don't get collected prematurely.
    command_line->AppendSwitchASCII(switches::kJavaScriptFlags, "--expose-gc");
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    host_resolver()->AddRule(kWebstoreDomain, "127.0.0.1");
    host_resolver()->AddRule(kAppDomain, "127.0.0.1");
    host_resolver()->AddRule(kNonAppDomain, "127.0.0.1");
  }

 protected:
  GURL GenerateTestServerUrl(const std::string& domain,
                             const std::string& page_filename) {
    GURL page_url = test_server()->GetURL(
        "files/extensions/api_test/webstore_inline_install/" + page_filename);

    GURL::Replacements replace_host;
    replace_host.SetHostStr(domain);
    return page_url.ReplaceComponents(replace_host);
  }

  void RunInlineInstallTest(const std::string& test_function_name) {
    bool result = false;
    std::string script = StringPrintf("%s('%s')", test_function_name.c_str(),
        test_gallery_url_.c_str());
    ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
        browser()->GetSelectedWebContents()->GetRenderViewHost(), L"",
        UTF8ToWide(script), &result));
    EXPECT_TRUE(result);
  }

  std::string test_gallery_url_;
};

IN_PROC_BROWSER_TEST_F(WebstoreInlineInstallTest, Install) {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAppsGalleryInstallAutoConfirmForTests, "accept");

  ui_test_utils::NavigateToURL(
      browser(), GenerateTestServerUrl(kAppDomain, "install.html"));

  RunInlineInstallTest("runTest");

  const Extension* extension = browser()->profile()->GetExtensionService()->
      GetExtensionById("ecglahbcnmdpdciemllbhojghbkagdje", false);
  EXPECT_TRUE(extension);
}

IN_PROC_BROWSER_TEST_F(WebstoreInlineInstallTest,
    InstallNotAllowedFromNonVerifiedDomains) {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAppsGalleryInstallAutoConfirmForTests, "cancel");
  ui_test_utils::NavigateToURL(
      browser(),
      GenerateTestServerUrl(kNonAppDomain, "install_non_verified_domain.html"));

  RunInlineInstallTest("runTest1");
  RunInlineInstallTest("runTest2");
}

IN_PROC_BROWSER_TEST_F(WebstoreInlineInstallTest, FindLink) {
  ui_test_utils::NavigateToURL(
      browser(), GenerateTestServerUrl(kAppDomain, "find_link.html"));

  RunInlineInstallTest("runTest");
}

IN_PROC_BROWSER_TEST_F(WebstoreInlineInstallTest, ArgumentValidation) {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAppsGalleryInstallAutoConfirmForTests, "cancel");
  ui_test_utils::NavigateToURL(
      browser(), GenerateTestServerUrl(kAppDomain, "argument_validation.html"));

  RunInlineInstallTest("runTest");
}

IN_PROC_BROWSER_TEST_F(WebstoreInlineInstallTest, InstallNotSupported) {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAppsGalleryInstallAutoConfirmForTests, "cancel");
  ui_test_utils::NavigateToURL(
      browser(),
      GenerateTestServerUrl(kAppDomain, "install_not_supported.html"));

  RunInlineInstallTest("runTest");

  // The inline install should fail, and a store-provided URL should be opened
  // in a new tab.
  if (browser()->tab_strip_model()->count() == 1) {
    ui_test_utils::WaitForNewTab(browser());
  }
  WebContents* web_contents = browser()->GetSelectedWebContents();
  EXPECT_EQ(GURL("http://cws.com/show-me-the-money"), web_contents->GetURL());
}

// The unpack failure test needs to use a different install .crx, which is
// specified via a command-line flag, so it needs its own test subclass.
class WebstoreInlineInstallUnpackFailureTest
    : public WebstoreInlineInstallTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    WebstoreInlineInstallTest::SetUpCommandLine(command_line);

    GURL crx_url = GenerateTestServerUrl(
        kWebstoreDomain, "malformed_extension.crx");
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kAppsGalleryUpdateURL, crx_url.spec());
  }

  void SetUpInProcessBrowserTestFixture() OVERRIDE {
    WebstoreInlineInstallTest::SetUpInProcessBrowserTestFixture();
    ExtensionInstallUI::DisableFailureUIForTests();
  }
};

IN_PROC_BROWSER_TEST_F(WebstoreInlineInstallUnpackFailureTest,
    WebstoreInlineInstallUnpackFailureTest) {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAppsGalleryInstallAutoConfirmForTests, "accept");

  ui_test_utils::NavigateToURL(browser(),
      GenerateTestServerUrl(kAppDomain, "install_unpack_failure.html"));

  RunInlineInstallTest("runTest");
}
