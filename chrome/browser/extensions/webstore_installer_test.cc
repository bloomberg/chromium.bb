// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/extensions/webstore_inline_installer.h"
#include "chrome/browser/extensions/webstore_inline_installer_factory.h"
#include "chrome/browser/extensions/webstore_installer_test.h"
#include "chrome/browser/extensions/webstore_standalone_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/host_port_pair.h"
#include "net/dns/mock_host_resolver.h"
#include "url/gurl.h"

using content::WebContents;
using extensions::Extension;
using extensions::TabHelper;
using extensions::WebstoreInlineInstaller;
using extensions::WebstoreInlineInstallerFactory;
using extensions::WebstoreStandaloneInstaller;

WebstoreInstallerTest::WebstoreInstallerTest(
    const std::string& webstore_domain,
    const std::string& test_data_path,
    const std::string& crx_filename,
    const std::string& verified_domain,
    const std::string& unverified_domain)
    : webstore_domain_(webstore_domain),
      test_data_path_(test_data_path),
      crx_filename_(crx_filename),
      verified_domain_(verified_domain),
      unverified_domain_(unverified_domain) {
}

WebstoreInstallerTest::~WebstoreInstallerTest() {}

void WebstoreInstallerTest::SetUpCommandLine(CommandLine* command_line) {
  ExtensionBrowserTest::SetUpCommandLine(command_line);
  // We start the test server now instead of in
  // SetUpInProcessBrowserTestFixture so that we can get its port number.
  ASSERT_TRUE(test_server()->Start());

  net::HostPortPair host_port = test_server()->host_port_pair();
  test_gallery_url_ = base::StringPrintf(
      "http://%s:%d/files/%s",
      webstore_domain_.c_str(), host_port.port(), test_data_path_.c_str());
  command_line->AppendSwitchASCII(
      switches::kAppsGalleryURL, test_gallery_url_);

  GURL crx_url = GenerateTestServerUrl(webstore_domain_, crx_filename_);
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAppsGalleryUpdateURL, crx_url.spec());

  // Allow tests to call window.gc(), so that we can check that callback
  // functions don't get collected prematurely.
  command_line->AppendSwitchASCII(switches::kJavaScriptFlags, "--expose-gc");
}

void WebstoreInstallerTest::SetUpInProcessBrowserTestFixture() {
  host_resolver()->AddRule(webstore_domain_, "127.0.0.1");
  host_resolver()->AddRule(verified_domain_, "127.0.0.1");
  host_resolver()->AddRule(unverified_domain_, "127.0.0.1");
}

void WebstoreInstallerTest::SetUpOnMainThread() {
  ExtensionBrowserTest::SetUpOnMainThread();
  ASSERT_TRUE(download_directory_.CreateUniqueTempDir());
  DownloadPrefs* download_prefs = DownloadPrefs::FromBrowserContext(
      browser()->profile());
  download_prefs->SetDownloadPath(download_directory_.path());
}

GURL WebstoreInstallerTest::GenerateTestServerUrl(
    const std::string& domain,
    const std::string& page_filename) {
  GURL page_url = test_server()->GetURL(
      base::StringPrintf("files/%s/%s",
                         test_data_path_.c_str(),
                         page_filename.c_str()));

  GURL::Replacements replace_host;
  replace_host.SetHostStr(domain);
  return page_url.ReplaceComponents(replace_host);
}

void WebstoreInstallerTest::RunTest(const std::string& test_function_name) {
  bool result = false;
  std::string script = base::StringPrintf(
      "%s('%s')", test_function_name.c_str(),
      test_gallery_url_.c_str());
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      script,
      &result));
  EXPECT_TRUE(result);
}

bool WebstoreInstallerTest::RunIndexedTest(
    const std::string& test_function_name,
    int i) {
  std::string result = "FAILED";
  std::string script = base::StringPrintf("%s('%s', %d)",
      test_function_name.c_str(), test_gallery_url_.c_str(), i);
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      script,
      &result));
  EXPECT_TRUE(result != "FAILED");
  return result == "KEEPGOING";
}

void WebstoreInstallerTest::RunTestAsync(
    const std::string& test_function_name) {
  std::string script = base::StringPrintf(
      "%s('%s')", test_function_name.c_str(), test_gallery_url_.c_str());
  browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame()->
      ExecuteJavaScript(base::UTF8ToUTF16(script));
}

void WebstoreInstallerTest::AutoAcceptInstall() {
  // TODO(tmdiep): Refactor and remove the use of the command line flag.
  // See crbug.com/357774.
  ExtensionInstallPrompt::g_auto_confirm_for_tests =
      ExtensionInstallPrompt::ACCEPT;
}

void WebstoreInstallerTest::AutoCancelInstall() {
  ExtensionInstallPrompt::g_auto_confirm_for_tests =
      ExtensionInstallPrompt::CANCEL;
}
