// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/mock_host_resolver.h"

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, IncognitoNoScript) {
  host_resolver()->AddRule("*", "127.0.0.1");
  StartHTTPServer();

  // Loads a simple extension which attempts to change the title of every page
  // that loads to "modified".
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("api_test")
      .AppendASCII("incognito_no_script")));

  // Open incognito window and navigate to test page.
  ui_test_utils::OpenURLOffTheRecord(browser()->profile(),
      GURL("http://www.example.com:1337/files/extensions/test_file.html"));
  Browser* otr_browser = BrowserList::FindBrowserWithType(
      browser()->profile()->GetOffTheRecordProfile(), Browser::TYPE_NORMAL);
  TabContents* tab = otr_browser->GetSelectedTabContents();

  // Verify the script didn't run.
  bool result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), L"",
      L"window.domAutomationController.send(document.title == 'Unmodified')",
      &result);
  EXPECT_TRUE(result);
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, IncognitoYesScript) {
  host_resolver()->AddRule("*", "127.0.0.1");
  StartHTTPServer();

  // Load a dummy extension. This just tests that we don't regress a
  // crash fix when multiple incognito- and non-incognito-enabled extensions
  // are mixed.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("api_test")
      .AppendASCII("content_scripts").AppendASCII("all_frames")));

  // Loads a simple extension which attempts to change the title of every page
  // that loads to "modified".
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("api_test")
      .AppendASCII("incognito_no_script")));

  // Dummy extension #2.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("api_test")
      .AppendASCII("content_scripts").AppendASCII("isolated_world1")));

  // Now enable the incognito_no_script extension in incognito mode, and ensure
  // that page titles are modified.
  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  service->extension_prefs()->SetIsIncognitoEnabled(
      service->extensions()->at(1)->id(), true);
  browser()->profile()->GetUserScriptMaster()->ReloadExtensionForTesting(
      service->extensions()->at(1));

  // Open incognito window and navigate to test page.
  ui_test_utils::OpenURLOffTheRecord(browser()->profile(),
      GURL("http://www.example.com:1337/files/extensions/test_file.html"));
  Browser* otr_browser = BrowserList::FindBrowserWithType(
      browser()->profile()->GetOffTheRecordProfile(), Browser::TYPE_NORMAL);
  TabContents* tab = otr_browser->GetSelectedTabContents();

  // Verify the script ran.
  bool result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), L"",
      L"window.domAutomationController.send(document.title == 'modified')",
      &result);
  EXPECT_TRUE(result);
}
