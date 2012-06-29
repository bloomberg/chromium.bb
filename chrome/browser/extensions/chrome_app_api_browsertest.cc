// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"
#include "net/base/mock_host_resolver.h"

using extensions::Extension;

class ChromeAppAPITest : public ExtensionBrowserTest {
 protected:
  bool IsAppInstalled() { return IsAppInstalled(L""); }
  bool IsAppInstalled(const std::wstring& frame_xpath) {
    std::wstring get_app_is_installed =
        L"window.domAutomationController.send(window.chrome.app.isInstalled);";
    bool result;
    CHECK(
        ui_test_utils::ExecuteJavaScriptAndExtractBool(
            chrome::GetActiveWebContents(browser())->GetRenderViewHost(),
            frame_xpath, get_app_is_installed, &result));
    return result;
  }

  std::string InstallState() { return InstallState(L""); }
  std::string InstallState(const std::wstring& frame_xpath) {
    std::wstring get_app_install_state =
        L"window.chrome.app.installState("
        L"function(s) { window.domAutomationController.send(s); });";
    std::string result;
    CHECK(
        ui_test_utils::ExecuteJavaScriptAndExtractString(
            chrome::GetActiveWebContents(browser())->GetRenderViewHost(),
            frame_xpath, get_app_install_state, &result));
    return result;
  }

  std::string RunningState() { return RunningState(L""); }
  std::string RunningState(const std::wstring& frame_xpath) {
    std::wstring get_app_install_state =
        L"window.domAutomationController.send("
        L"window.chrome.app.runningState());";
    std::string result;
    CHECK(
        ui_test_utils::ExecuteJavaScriptAndExtractString(
            chrome::GetActiveWebContents(browser())->GetRenderViewHost(),
            frame_xpath, get_app_install_state, &result));
    return result;
  }

 private:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kAppsCheckoutURL,
                                    "http://checkout.com:");
  }
};

IN_PROC_BROWSER_TEST_F(ChromeAppAPITest, IsInstalled) {
  std::string app_host("app.com");
  std::string nonapp_host("nonapp.com");

  host_resolver()->AddRule(app_host, "127.0.0.1");
  host_resolver()->AddRule(nonapp_host, "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  GURL test_file_url(test_server()->GetURL("extensions/test_file.html"));
  GURL::Replacements replace_host;

  replace_host.SetHostStr(app_host);
  GURL app_url(test_file_url.ReplaceComponents(replace_host));

  replace_host.SetHostStr(nonapp_host);
  GURL non_app_url(test_file_url.ReplaceComponents(replace_host));

  // Before the app is installed, app.com does not think that it is installed
  ui_test_utils::NavigateToURL(browser(), app_url);
  EXPECT_FALSE(IsAppInstalled());

  // Load an app which includes app.com in its extent.
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("app_dot_com_app"));
  ASSERT_TRUE(extension);

  // Even after the app is installed, the existing app.com tab is not in an
  // app process, so chrome.app.isInstalled should return false.
  EXPECT_FALSE(IsAppInstalled());

  // Test that a non-app page has chrome.app.isInstalled = false.
  ui_test_utils::NavigateToURL(browser(), non_app_url);
  EXPECT_FALSE(IsAppInstalled());

  // Test that a non-app page returns null for chrome.app.getDetails().
  std::wstring get_app_details =
      L"window.domAutomationController.send("
      L"    JSON.stringify(window.chrome.app.getDetails()));";
  std::string result;
  ASSERT_TRUE(
      ui_test_utils::ExecuteJavaScriptAndExtractString(
          chrome::GetActiveWebContents(browser())->GetRenderViewHost(),
          L"", get_app_details, &result));
  EXPECT_EQ("null", result);

  // Check that an app page has chrome.app.isInstalled = true.
  ui_test_utils::NavigateToURL(browser(), app_url);
  EXPECT_TRUE(IsAppInstalled());

  // Check that an app page returns the correct result for
  // chrome.app.getDetails().
  ui_test_utils::NavigateToURL(browser(), app_url);
  ASSERT_TRUE(
      ui_test_utils::ExecuteJavaScriptAndExtractString(
          chrome::GetActiveWebContents(browser())->GetRenderViewHost(),
          L"", get_app_details, &result));
  scoped_ptr<DictionaryValue> app_details(
      static_cast<DictionaryValue*>(base::JSONReader::Read(result)));
  // extension->manifest() does not contain the id.
  app_details->Remove("id", NULL);
  EXPECT_TRUE(app_details.get());
  EXPECT_TRUE(app_details->Equals(extension->manifest()->value()));

  // Try to change app.isInstalled.  Should silently fail, so
  // that isInstalled should have the initial value.
  ASSERT_TRUE(
      ui_test_utils::ExecuteJavaScriptAndExtractString(
          chrome::GetActiveWebContents(browser())->GetRenderViewHost(),
          L"",
          L"window.domAutomationController.send("
          L"    function() {"
          L"        var value = window.chrome.app.isInstalled;"
          L"        window.chrome.app.isInstalled = !value;"
          L"        if (window.chrome.app.isInstalled == value) {"
          L"            return 'true';"
          L"        } else {"
          L"            return 'false';"
          L"        }"
          L"    }()"
          L");",
         &result));

  // Should not be able to alter window.chrome.app.isInstalled from javascript";
  EXPECT_EQ("true", result);
}

IN_PROC_BROWSER_TEST_F(ChromeAppAPITest, GetDetailsForFrame) {
  std::string app_host("app.com");
  std::string nonapp_host("nonapp.com");
  std::string checkout_host("checkout.com");

  host_resolver()->AddRule(app_host, "127.0.0.1");
  host_resolver()->AddRule(nonapp_host, "127.0.0.1");
  host_resolver()->AddRule(checkout_host, "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  GURL test_file_url(test_server()->GetURL(
      "files/extensions/get_app_details_for_frame.html"));
  GURL::Replacements replace_host;

  replace_host.SetHostStr(checkout_host);
  GURL checkout_url(test_file_url.ReplaceComponents(replace_host));

  replace_host.SetHostStr(app_host);
  GURL app_url(test_file_url.ReplaceComponents(replace_host));

  // Load an app which includes app.com in its extent.
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("app_dot_com_app"));
  ASSERT_TRUE(extension);

  // Test that normal pages (even apps) cannot use getDetailsForFrame().
  ui_test_utils::NavigateToURL(browser(), app_url);
  std::wstring test_unsuccessful_access =
      L"window.domAutomationController.send(window.testUnsuccessfulAccess())";
  bool result = false;
  ASSERT_TRUE(
      ui_test_utils::ExecuteJavaScriptAndExtractBool(
          chrome::GetActiveWebContents(browser())->GetRenderViewHost(),
          L"", test_unsuccessful_access, &result));
  EXPECT_TRUE(result);

  // Test that checkout can use getDetailsForFrame() and that it works
  // correctly.
  ui_test_utils::NavigateToURL(browser(), checkout_url);
  std::wstring get_details_for_frame =
      L"window.domAutomationController.send("
      L"    JSON.stringify(chrome.app.getDetailsForFrame(frames[0])))";
  std::string json;
  ASSERT_TRUE(
      ui_test_utils::ExecuteJavaScriptAndExtractString(
          chrome::GetActiveWebContents(browser())->GetRenderViewHost(),
          L"", get_details_for_frame, &json));

  scoped_ptr<DictionaryValue> app_details(
      static_cast<DictionaryValue*>(base::JSONReader::Read(json)));
  // extension->manifest() does not contain the id.
  app_details->Remove("id", NULL);
  EXPECT_TRUE(app_details.get());
  EXPECT_TRUE(app_details->Equals(extension->manifest()->value()));
}


IN_PROC_BROWSER_TEST_F(ChromeAppAPITest, InstallAndRunningState) {
  std::string app_host("app.com");
  std::string non_app_host("nonapp.com");

  host_resolver()->AddRule(app_host, "127.0.0.1");
  host_resolver()->AddRule(non_app_host, "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  GURL test_file_url(test_server()->GetURL(
      "files/extensions/get_app_details_for_frame.html"));
  GURL::Replacements replace_host;

  replace_host.SetHostStr(app_host);
  GURL app_url(test_file_url.ReplaceComponents(replace_host));

  replace_host.SetHostStr(non_app_host);
  GURL non_app_url(test_file_url.ReplaceComponents(replace_host));

  // Before the app is installed, app.com does not think that it is installed
  ui_test_utils::NavigateToURL(browser(), app_url);

  EXPECT_EQ("not_installed", InstallState());
  EXPECT_EQ("cannot_run", RunningState());
  EXPECT_FALSE(IsAppInstalled());

  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("app_dot_com_app"));
  ASSERT_TRUE(extension);

  EXPECT_EQ("installed", InstallState());
  EXPECT_EQ("ready_to_run", RunningState());
  EXPECT_FALSE(IsAppInstalled());

  // Reloading the page should put the tab in an app process.
  ui_test_utils::NavigateToURL(browser(), app_url);
  EXPECT_EQ("installed", InstallState());
  EXPECT_EQ("running", RunningState());
  EXPECT_TRUE(IsAppInstalled());

  // Disable the extension and verify the state.
  browser()->profile()->GetExtensionService()->DisableExtension(
      extension->id(), Extension::DISABLE_PERMISSIONS_INCREASE);
  ui_test_utils::NavigateToURL(browser(), app_url);

  EXPECT_EQ("disabled", InstallState());
  EXPECT_EQ("cannot_run", RunningState());
  EXPECT_FALSE(IsAppInstalled());

  browser()->profile()->GetExtensionService()->EnableExtension(extension->id());
  EXPECT_EQ("installed", InstallState());
  EXPECT_EQ("ready_to_run", RunningState());
  EXPECT_FALSE(IsAppInstalled());

  // The non-app URL should still not be installed or running.
  ui_test_utils::NavigateToURL(browser(), non_app_url);

  EXPECT_EQ("not_installed", InstallState());
  EXPECT_EQ("cannot_run", RunningState());
  EXPECT_FALSE(IsAppInstalled());

  EXPECT_EQ("installed", InstallState(L"//html/iframe[1]"));
  EXPECT_EQ("cannot_run", RunningState(L"//html/iframe[1]"));
  EXPECT_FALSE(IsAppInstalled(L"//html/iframe[1]"));

}

IN_PROC_BROWSER_TEST_F(ChromeAppAPITest, InstallAndRunningStateFrame) {
  std::string app_host("app.com");
  std::string non_app_host("nonapp.com");

  host_resolver()->AddRule(app_host, "127.0.0.1");
  host_resolver()->AddRule(non_app_host, "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  GURL test_file_url(test_server()->GetURL(
      "files/extensions/get_app_details_for_frame_reversed.html"));
  GURL::Replacements replace_host;

  replace_host.SetHostStr(app_host);
  GURL app_url(test_file_url.ReplaceComponents(replace_host));

  replace_host.SetHostStr(non_app_host);
  GURL non_app_url(test_file_url.ReplaceComponents(replace_host));

  // Check the install and running state of a non-app iframe running
  // within an app.
  ui_test_utils::NavigateToURL(browser(), app_url);

  EXPECT_EQ("not_installed", InstallState(L"//html/iframe[1]"));
  EXPECT_EQ("cannot_run", RunningState(L"//html/iframe[1]"));
  EXPECT_FALSE(IsAppInstalled(L"//html/iframe[1]"));
}
