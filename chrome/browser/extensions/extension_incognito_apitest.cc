// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
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
  ASSERT_TRUE(StartHTTPServer());

  // Loads a simple extension which attempts to change the title of every page
  // that loads to "modified".
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("api_test")
      .AppendASCII("incognito").AppendASCII("content_scripts")));

  // Open incognito window and navigate to test page.
  ui_test_utils::OpenURLOffTheRecord(browser()->profile(),
      GURL("http://www.example.com:1337/files/extensions/test_file.html"));
  Browser* otr_browser = BrowserList::FindBrowserWithType(
      browser()->profile()->GetOffTheRecordProfile(), Browser::TYPE_NORMAL,
      false);
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
  ASSERT_TRUE(StartHTTPServer());

  // Load a dummy extension. This just tests that we don't regress a
  // crash fix when multiple incognito- and non-incognito-enabled extensions
  // are mixed.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("api_test")
      .AppendASCII("content_scripts").AppendASCII("all_frames")));

  // Loads a simple extension which attempts to change the title of every page
  // that loads to "modified".
  ASSERT_TRUE(LoadExtensionIncognito(test_data_dir_.AppendASCII("api_test")
      .AppendASCII("incognito").AppendASCII("content_scripts")));

  // Dummy extension #2.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("api_test")
      .AppendASCII("content_scripts").AppendASCII("isolated_world1")));

  // Open incognito window and navigate to test page.
  ui_test_utils::OpenURLOffTheRecord(browser()->profile(),
      GURL("http://www.example.com:1337/files/extensions/test_file.html"));
  Browser* otr_browser = BrowserList::FindBrowserWithType(
      browser()->profile()->GetOffTheRecordProfile(), Browser::TYPE_NORMAL,
      false);
  TabContents* tab = otr_browser->GetSelectedTabContents();

  // Verify the script ran.
  bool result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), L"",
      L"window.domAutomationController.send(document.title == 'modified')",
      &result);
  EXPECT_TRUE(result);
}

// Tests that the APIs in an incognito-enabled extension work properly.
// Flaky, http://crbug.com/42844.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, FLAKY_Incognito) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(StartHTTPServer());

  ResultCatcher catcher;

  // Open incognito window and navigate to test page.
  ui_test_utils::OpenURLOffTheRecord(browser()->profile(),
      GURL("http://www.example.com:1337/files/extensions/test_file.html"));

  ASSERT_TRUE(LoadExtensionIncognito(test_data_dir_
      .AppendASCII("incognito").AppendASCII("apis")));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Tests that the APIs in an incognito-disabled extension don't see incognito
// events or callbacks.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, IncognitoDisabled) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(StartHTTPServer());

  ResultCatcher catcher;

  // Open incognito window and navigate to test page.
  ui_test_utils::OpenURLOffTheRecord(browser()->profile(),
      GURL("http://www.example.com:1337/files/extensions/test_file.html"));

  ASSERT_TRUE(LoadExtension(test_data_dir_
      .AppendASCII("incognito").AppendASCII("apis_disabled")));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Test that opening a popup from an incognito browser window works properly.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, IncognitoPopup) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(StartHTTPServer());

  ResultCatcher catcher;

  ASSERT_TRUE(LoadExtensionIncognito(test_data_dir_
      .AppendASCII("incognito").AppendASCII("popup")));

  // Open incognito window and navigate to test page.
  ui_test_utils::OpenURLOffTheRecord(browser()->profile(),
      GURL("http://www.example.com:1337/files/extensions/test_file.html"));
  Browser* incognito_browser = BrowserList::FindBrowserWithType(
      browser()->profile()->GetOffTheRecordProfile(), Browser::TYPE_NORMAL,
      false);

  // Simulate the incognito's browser action being clicked.
  BrowserActionTestUtil(incognito_browser).Press(0);

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}
