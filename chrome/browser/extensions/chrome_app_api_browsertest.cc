// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"
#include "net/base/mock_host_resolver.h"

class ChromeAppAPITest : public ExtensionBrowserTest {
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


  // Load an app which includes app.com in its extent.
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("app_dot_com_app"));
  ASSERT_TRUE(extension);


  // Test that a non-app page has chrome.app.isInstalled = false.
  ui_test_utils::NavigateToURL(browser(), non_app_url);
  std::wstring get_app_is_installed =
      L"window.domAutomationController.send("
      L"    JSON.stringify(window.chrome.app.isInstalled));";
  std::string result;
  ASSERT_TRUE(
      ui_test_utils::ExecuteJavaScriptAndExtractString(
          browser()->GetSelectedTabContents()->render_view_host(),
          L"", get_app_is_installed, &result));
  EXPECT_EQ("false", result);

  // Test that a non-app page returns null for chrome.app.getDetails().
  std::wstring get_app_details =
      L"window.domAutomationController.send("
      L"    JSON.stringify(window.chrome.app.getDetails()));";
  ASSERT_TRUE(
      ui_test_utils::ExecuteJavaScriptAndExtractString(
          browser()->GetSelectedTabContents()->render_view_host(),
          L"", get_app_details, &result));
  EXPECT_EQ("null", result);

  // Check that an app page has chrome.app.isInstalled = true.
  ui_test_utils::NavigateToURL(browser(), app_url);
  ASSERT_TRUE(
      ui_test_utils::ExecuteJavaScriptAndExtractString(
          browser()->GetSelectedTabContents()->render_view_host(),
          L"", get_app_is_installed, &result));
  EXPECT_EQ("true", result);

  // Check that an app page returns the correct result for
  // chrome.app.getDetails().
  ui_test_utils::NavigateToURL(browser(), app_url);
  ASSERT_TRUE(
      ui_test_utils::ExecuteJavaScriptAndExtractString(
          browser()->GetSelectedTabContents()->render_view_host(),
          L"", get_app_details, &result));
  scoped_ptr<DictionaryValue> app_details(
      static_cast<DictionaryValue*>(
          base::JSONReader::Read(result, false /* allow trailing comma */)));
  // extension->manifest_value() does not contain the id.
  app_details->Remove("id", NULL);
  EXPECT_TRUE(app_details.get());
  EXPECT_TRUE(app_details->Equals(extension->manifest_value()));


  // Test that trying to set window.chrome.app.isInstalled throws
  // an exception.
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
          browser()->GetSelectedTabContents()->render_view_host(),
          L"",
          L"window.domAutomationController.send("
          L"    function() {"
          L"      try {"
          L"        window.chrome.app.isInstalled = false;"
          L"        return 'BAD: Should have thrown by now...';"
          L"      } catch (e) {"
          L"        return 'GOOD: Saw expected error.';"
          L"      }"
          L"    }()"
          L");",
          &result));
  EXPECT_EQ("GOOD: Saw expected error.", result);
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
          browser()->GetSelectedTabContents()->render_view_host(),
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
          browser()->GetSelectedTabContents()->render_view_host(),
          L"", get_details_for_frame, &json));

  scoped_ptr<DictionaryValue> app_details(
      static_cast<DictionaryValue*>(
          base::JSONReader::Read(json, false /* allow trailing comma */)));
  // extension->manifest_value() does not contain the id.
  app_details->Remove("id", NULL);
  EXPECT_TRUE(app_details.get());
  EXPECT_TRUE(app_details->Equals(extension->manifest_value()));
}
