// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/startup_helper.h"
#include "chrome/browser/extensions/webstore_standalone_installer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_builder.h"
#include "chrome/common/extensions/value_builder.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "googleurl/src/gurl.h"
#include "net/base/host_port_pair.h"
#include "net/base/mock_host_resolver.h"

using content::WebContents;
using extensions::DictionaryBuilder;
using extensions::Extension;
using extensions::ExtensionBuilder;
using extensions::ListBuilder;

const char kWebstoreDomain[] = "cws.com";
const char kAppDomain[] = "app.com";
const char kNonAppDomain[] = "nonapp.com";
const char kTestExtensionId[] = "ecglahbcnmdpdciemllbhojghbkagdje";

class WebstoreStandaloneInstallTest : public InProcessBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // We start the test server now instead of in
    // SetUpInProcessBrowserTestFixture so that we can get its port number.
    ASSERT_TRUE(test_server()->Start());

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

  void RunTest(const std::string& test_function_name) {
    bool result = false;
    std::string script = StringPrintf("%s('%s')", test_function_name.c_str(),
        test_gallery_url_.c_str());
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        chrome::GetActiveWebContents(browser()),
        script,
        &result));
    EXPECT_TRUE(result);
  }

  std::string test_gallery_url_;
};

IN_PROC_BROWSER_TEST_F(WebstoreStandaloneInstallTest, Install) {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAppsGalleryInstallAutoConfirmForTests, "accept");

  ui_test_utils::NavigateToURL(
      browser(), GenerateTestServerUrl(kAppDomain, "install.html"));

  RunTest("runTest");

  const extensions::Extension* extension = browser()->profile()->
      GetExtensionService()->GetExtensionById(kTestExtensionId, false);
  EXPECT_TRUE(extension);
}

IN_PROC_BROWSER_TEST_F(WebstoreStandaloneInstallTest,
    InstallNotAllowedFromNonVerifiedDomains) {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAppsGalleryInstallAutoConfirmForTests, "cancel");
  ui_test_utils::NavigateToURL(
      browser(),
      GenerateTestServerUrl(kNonAppDomain, "install_non_verified_domain.html"));

  RunTest("runTest1");
  RunTest("runTest2");
}

IN_PROC_BROWSER_TEST_F(WebstoreStandaloneInstallTest, FindLink) {
  ui_test_utils::NavigateToURL(
      browser(), GenerateTestServerUrl(kAppDomain, "find_link.html"));

  RunTest("runTest");
}

IN_PROC_BROWSER_TEST_F(WebstoreStandaloneInstallTest, ArgumentValidation) {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAppsGalleryInstallAutoConfirmForTests, "cancel");
  ui_test_utils::NavigateToURL(
      browser(), GenerateTestServerUrl(kAppDomain, "argument_validation.html"));

  RunTest("runTest");
}

IN_PROC_BROWSER_TEST_F(WebstoreStandaloneInstallTest, InstallNotSupported) {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAppsGalleryInstallAutoConfirmForTests, "cancel");
  ui_test_utils::NavigateToURL(
      browser(),
      GenerateTestServerUrl(kAppDomain, "install_not_supported.html"));

  RunTest("runTest");

  // The inline install should fail, and a store-provided URL should be opened
  // in a new tab.
  if (browser()->tab_strip_model()->count() == 1) {
    ui_test_utils::WaitForNewTab(browser());
  }
  WebContents* web_contents = chrome::GetActiveWebContents(browser());
  EXPECT_EQ(GURL("http://cws.com/show-me-the-money"), web_contents->GetURL());
}

// Regression test for http://crbug.com/144991.
IN_PROC_BROWSER_TEST_F(WebstoreStandaloneInstallTest, InstallFromHostedApp) {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAppsGalleryInstallAutoConfirmForTests, "accept");

  const GURL kInstallUrl = GenerateTestServerUrl(kAppDomain, "install.html");

  // We're forced to construct a hosted app dynamically because we need the
  // app to run on a declared URL, but we don't know the port ahead of time.
  scoped_refptr<const Extension> hosted_app = ExtensionBuilder()
      .SetManifest(DictionaryBuilder()
          .Set("name", "hosted app")
          .Set("version", "1")
          .Set("app", DictionaryBuilder()
              .Set("urls", ListBuilder().Append(kInstallUrl.spec()))
              .Set("launch", DictionaryBuilder()
                  .Set("web_url", kInstallUrl.spec())))
          .Set("manifest_version", 2))
      .Build();
  ASSERT_TRUE(hosted_app);

  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(browser()->profile())->
          extension_service();

  extension_service->AddExtension(hosted_app.get());
  EXPECT_TRUE(extension_service->extensions()->Contains(hosted_app->id()));

  ui_test_utils::NavigateToURL(browser(), kInstallUrl);

  EXPECT_FALSE(extension_service->extensions()->Contains(kTestExtensionId));
  RunTest("runTest");
  EXPECT_TRUE(extension_service->extensions()->Contains(kTestExtensionId));
}

// The unpack failure test needs to use a different install .crx, which is
// specified via a command-line flag, so it needs its own test subclass.
class WebstoreStandaloneInstallUnpackFailureTest
    : public WebstoreStandaloneInstallTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    WebstoreStandaloneInstallTest::SetUpCommandLine(command_line);

    GURL crx_url = GenerateTestServerUrl(
        kWebstoreDomain, "malformed_extension.crx");
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kAppsGalleryUpdateURL, crx_url.spec());
  }

  void SetUpInProcessBrowserTestFixture() OVERRIDE {
    WebstoreStandaloneInstallTest::SetUpInProcessBrowserTestFixture();
    ExtensionInstallUI::DisableFailureUIForTests();
  }
};

IN_PROC_BROWSER_TEST_F(WebstoreStandaloneInstallUnpackFailureTest,
    WebstoreStandaloneInstallUnpackFailureTest) {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAppsGalleryInstallAutoConfirmForTests, "accept");

  ui_test_utils::NavigateToURL(browser(),
      GenerateTestServerUrl(kAppDomain, "install_unpack_failure.html"));

  RunTest("runTest");
}

class CommandLineWebstoreInstall : public WebstoreStandaloneInstallTest,
                                   public content::NotificationObserver {
 public:
  CommandLineWebstoreInstall() : saw_install_(false), browser_open_count_(0) {}
  virtual ~CommandLineWebstoreInstall() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    WebstoreStandaloneInstallTest::SetUpOnMainThread();
    registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_INSTALLED,
                   content::NotificationService::AllSources());
    registrar_.Add(this, chrome::NOTIFICATION_BROWSER_OPENED,
                   content::NotificationService::AllSources());
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kInstallFromWebstore, kTestExtensionId);
  }

  bool saw_install() { return saw_install_; }

  int browser_open_count() { return browser_open_count_; }

  // NotificationObserver interface.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details)  OVERRIDE {
    if (type == chrome::NOTIFICATION_EXTENSION_INSTALLED) {
      const Extension* extension = content::Details<Extension>(details).ptr();
      ASSERT_TRUE(extension != NULL);
      EXPECT_EQ(extension->id(), kTestExtensionId);
      saw_install_ = true;
    } else if (type == chrome::NOTIFICATION_BROWSER_OPENED) {
      browser_open_count_++;
    } else {
      ASSERT_TRUE(false) << "Unexpected notification type : " << type;
    }
  }

  content::NotificationRegistrar registrar_;

  // Have we seen an installation notification for kTestExtensionId ?
  bool saw_install_;

  // How many NOTIFICATION_BROWSER_OPENED notifications have we seen?
  int browser_open_count_;
};

IN_PROC_BROWSER_TEST_F(CommandLineWebstoreInstall, Accept) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(
      switches::kAppsGalleryInstallAutoConfirmForTests, "accept");
  extensions::StartupHelper helper;
  EXPECT_TRUE(helper.InstallFromWebstore(*command_line, browser()->profile()));
  EXPECT_TRUE(saw_install());
  EXPECT_EQ(0, browser_open_count());
}

IN_PROC_BROWSER_TEST_F(CommandLineWebstoreInstall, Cancel) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(
      switches::kAppsGalleryInstallAutoConfirmForTests, "cancel");
  extensions::StartupHelper helper;
  EXPECT_FALSE(helper.InstallFromWebstore(*command_line, browser()->profile()));
  EXPECT_FALSE(saw_install());
  EXPECT_EQ(0, browser_open_count());
}
