// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_install_dialog.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/webstore_inline_installer.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/content_notification_types.h"
#include "googleurl/src/gurl.h"
#include "net/base/host_port_pair.h"
#include "net/base/mock_host_resolver.h"

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

    command_line->AppendSwitch(switches::kEnableInlineWebstoreInstall);
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

  void RunInlineInstallTest() {
    bool result = false;
    std::string script =
        StringPrintf("runTest('%s')", test_gallery_url_.c_str());
    ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
        browser()->GetSelectedTabContents()->render_view_host(), L"",
        UTF8ToWide(script), &result));
    EXPECT_TRUE(result);
  }

  std::string test_gallery_url_;
};

// Flakily fails on linux.  http://crbug.com/95246
#if defined(OS_LINUX)
#define MAYBE_Install DISABLED_Install
#else
#define MAYBE_Install Install
#endif

IN_PROC_BROWSER_TEST_F(WebstoreInlineInstallTest, MAYBE_Install) {
  SetExtensionInstallDialogForManifestAutoConfirmForTests(true);

  ui_test_utils::WindowedNotificationObserver load_signal(
        chrome::NOTIFICATION_EXTENSION_LOADED,
        Source<Profile>(browser()->profile()));

  ui_test_utils::NavigateToURL(
      browser(), GenerateTestServerUrl(kAppDomain, "install.html"));

  RunInlineInstallTest();

  load_signal.Wait();

  const Extension* extension = browser()->profile()->GetExtensionService()->
      GetExtensionById("ecglahbcnmdpdciemllbhojghbkagdje", false);
  EXPECT_TRUE(extension != NULL);
}

// Flakily fails on Linux.  http://crbug.com/95246
#if defined(OS_LINUX)
#define MAYBE_InstallNotAllowedFromNonVerifiedDomains DISABLED_InstallNotAllowedFromNonVerifiedDomains
#else
#define MAYBE_InstallNotAllowedFromNonVerifiedDomains InstallNotAllowedFromNonVerifiedDomains
#endif

IN_PROC_BROWSER_TEST_F(
    WebstoreInlineInstallTest, MAYBE_InstallNotAllowedFromNonVerifiedDomains) {
  SetExtensionInstallDialogForManifestAutoConfirmForTests(false);
  ui_test_utils::NavigateToURL(
      browser(),
      GenerateTestServerUrl(kNonAppDomain, "install-non-verified-domain.html"));

  RunInlineInstallTest();
}

// Flakily fails on Linux.  http://crbug.com/95246
#if defined(OS_LINUX)
#define MAYBE_FindLink DISABLED_FindLink
#else
#define MAYBE_FindLink FindLink
#endif

IN_PROC_BROWSER_TEST_F(WebstoreInlineInstallTest, MAYBE_FindLink) {
  ui_test_utils::NavigateToURL(
      browser(), GenerateTestServerUrl(kAppDomain, "find_link.html"));

  RunInlineInstallTest();
}

IN_PROC_BROWSER_TEST_F(WebstoreInlineInstallTest, ArgumentValidation) {
  SetExtensionInstallDialogForManifestAutoConfirmForTests(false);
  ui_test_utils::NavigateToURL(
      browser(), GenerateTestServerUrl(kAppDomain, "argument_validation.html"));

  RunInlineInstallTest();
}
