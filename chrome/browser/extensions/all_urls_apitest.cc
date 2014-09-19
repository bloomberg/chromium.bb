// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extensions_client.h"
#include "extensions/test/extension_test_message_listener.h"

const std::string kAllUrlsTarget =
    "files/extensions/api_test/all_urls/index.html";

typedef ExtensionApiTest AllUrlsApiTest;

#if defined(OS_WIN) && !defined(NDEBUG)
// http://crbug.com/174341
#define MAYBE_WhitelistedExtension DISABLED_WhitelistedExtension
#else
#define MAYBE_WhitelistedExtension WhitelistedExtension
#endif
IN_PROC_BROWSER_TEST_F(AllUrlsApiTest, MAYBE_WhitelistedExtension) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  // First setup the two extensions.
  base::FilePath extension_dir1 = test_data_dir_.AppendASCII("all_urls")
                                          .AppendASCII("content_script");
  base::FilePath extension_dir2 = test_data_dir_.AppendASCII("all_urls")
                                          .AppendASCII("execute_script");

  // Then add the two extensions to the whitelist.
  extensions::ExtensionsClient::ScriptingWhitelist whitelist;
  whitelist.push_back(crx_file::id_util::GenerateIdForPath(extension_dir1));
  whitelist.push_back(crx_file::id_util::GenerateIdForPath(extension_dir2));
  extensions::ExtensionsClient::Get()->SetScriptingWhitelist(whitelist);

  // Then load extensions.
  ExtensionService* service = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service();
  const size_t size_before = service->extensions()->size();
  ASSERT_TRUE(LoadExtension(extension_dir1));
  ASSERT_TRUE(LoadExtension(extension_dir2));
  EXPECT_EQ(size_before + 2, service->extensions()->size());

  std::string url;

  // Now verify we run content scripts on chrome://newtab/.
  url = "chrome://newtab/";
  ExtensionTestMessageListener listener1a("content script: " + url, false);
  ExtensionTestMessageListener listener1b("execute: " + url, false);
  ui_test_utils::NavigateToURL(browser(), GURL(url));
  ASSERT_TRUE(listener1a.WaitUntilSatisfied());
  ASSERT_TRUE(listener1b.WaitUntilSatisfied());

  // Now verify data: urls.
  url = "data:text/html;charset=utf-8,<html>asdf</html>";
  ExtensionTestMessageListener listener2a("content script: " + url, false);
  ExtensionTestMessageListener listener2b("execute: " + url, false);
  ui_test_utils::NavigateToURL(browser(), GURL(url));
  ASSERT_TRUE(listener2a.WaitUntilSatisfied());
  ASSERT_TRUE(listener2b.WaitUntilSatisfied());

  // Now verify chrome://version/.
  url = "chrome://version/";
  ExtensionTestMessageListener listener3a("content script: " + url, false);
  ExtensionTestMessageListener listener3b("execute: " + url, false);
  ui_test_utils::NavigateToURL(browser(), GURL(url));
  ASSERT_TRUE(listener3a.WaitUntilSatisfied());
  ASSERT_TRUE(listener3b.WaitUntilSatisfied());

  // Now verify about:blank.
  url = "about:blank";
  ExtensionTestMessageListener listener4a("content script: " + url, false);
  ExtensionTestMessageListener listener4b("execute: " + url, false);
  ui_test_utils::NavigateToURL(browser(), GURL(url));
  ASSERT_TRUE(listener4a.WaitUntilSatisfied());
  ASSERT_TRUE(listener4b.WaitUntilSatisfied());

  // Now verify we can script a regular http page.
  ASSERT_TRUE(test_server()->Start());
  GURL page_url = test_server()->GetURL(kAllUrlsTarget);
  ExtensionTestMessageListener listener5a("content script: " + page_url.spec(),
                                          false);
  ExtensionTestMessageListener listener5b("execute: " + page_url.spec(), false);
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_TRUE(listener5a.WaitUntilSatisfied());
  ASSERT_TRUE(listener5b.WaitUntilSatisfied());
}

// Test that an extension NOT whitelisted for scripting can ask for <all_urls>
// and run scripts on non-restricted all pages.
IN_PROC_BROWSER_TEST_F(AllUrlsApiTest, RegularExtensions) {
  // First load the two extensions.
  base::FilePath extension_dir1 = test_data_dir_.AppendASCII("all_urls")
                                          .AppendASCII("content_script");
  base::FilePath extension_dir2 = test_data_dir_.AppendASCII("all_urls")
                                          .AppendASCII("execute_script");

  ExtensionService* service = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service();
  const size_t size_before = service->extensions()->size();
  ASSERT_TRUE(LoadExtension(extension_dir1));
  ASSERT_TRUE(LoadExtension(extension_dir2));
  EXPECT_EQ(size_before + 2, service->extensions()->size());

  // Now verify we can script a regular http page.
  ASSERT_TRUE(test_server()->Start());
  GURL page_url = test_server()->GetURL(kAllUrlsTarget);
  ExtensionTestMessageListener listener1a("content script: " + page_url.spec(),
                                          false);
  ExtensionTestMessageListener listener1b("execute: " + page_url.spec(), false);
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_TRUE(listener1a.WaitUntilSatisfied());
  ASSERT_TRUE(listener1b.WaitUntilSatisfied());
}
