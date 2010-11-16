// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/ui_test_utils.h"

const std::string kAllUrlsTarget =
    "files/extensions/api_test/all_urls/index.html";

typedef ExtensionApiTest AllUrlsApiTest;


// Note: This test is flaky, but is actively being worked on.
// See http://crbug.com/57694. Finnur is adding traces to figure out where the
// problem lies and needs to check in these traces because the problem doesn't
// repro locally (nor on the try bots).
IN_PROC_BROWSER_TEST_F(AllUrlsApiTest, WhitelistedExtension) {
  // First setup the two extensions.
  FilePath extension_dir1 = test_data_dir_.AppendASCII("all_urls")
                                          .AppendASCII("content_script");
  FilePath extension_dir2 = test_data_dir_.AppendASCII("all_urls")
                                          .AppendASCII("execute_script");

  // Then add the two extensions to the whitelist.
  Extension::ScriptingWhitelist whitelist;
  whitelist.push_back(Extension::GenerateIdForPath(extension_dir1));
  whitelist.push_back(Extension::GenerateIdForPath(extension_dir2));
  Extension::SetScriptingWhitelist(whitelist);

  // Then load extensions.
  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  const size_t size_before = service->extensions()->size();
  printf("***** LoadExtension1 called \n");
  ASSERT_TRUE(LoadExtension(extension_dir1));
  printf("***** LoadExtension2 called \n");
  ASSERT_TRUE(LoadExtension(extension_dir2));
  printf("***** LoadExtensions done \n");
  EXPECT_EQ(size_before + 2, service->extensions()->size());

  std::string url;

  // Now verify we run content scripts on chrome://newtab/.
  url = "chrome://newtab/";
  printf("***** %s\n", url.c_str());
  ExtensionTestMessageListener listener1a("content script: " + url, false);
  ExtensionTestMessageListener listener1b("execute: " + url, false);
  ui_test_utils::NavigateToURL(browser(), GURL(url));
  printf("***** Wait 1a\n");
  ASSERT_TRUE(listener1a.WaitUntilSatisfied());
  printf("***** Wait 1b\n");
  ASSERT_TRUE(listener1b.WaitUntilSatisfied());

  // Now verify data: urls.
  url = "data:text/html;charset=utf-8,<html>asdf</html>";
  printf("***** %s\n", url.c_str());
  ExtensionTestMessageListener listener2a("content script: " + url, false);
  ExtensionTestMessageListener listener2b("execute: " + url, false);
  ui_test_utils::NavigateToURL(browser(), GURL(url));
  printf("***** Wait 2a\n");
  ASSERT_TRUE(listener2a.WaitUntilSatisfied());
  printf("***** Wait 2b\n");
  ASSERT_TRUE(listener2b.WaitUntilSatisfied());

  // Now verify about:version.
  url = "about:version";
  printf("***** %s\n", url.c_str());
  ExtensionTestMessageListener listener3a("content script: " + url, false);
  ExtensionTestMessageListener listener3b("execute: " + url, false);
  ui_test_utils::NavigateToURL(browser(), GURL(url));
  printf("***** Wait 3a\n");
  ASSERT_TRUE(listener3a.WaitUntilSatisfied());
  printf("***** Wait 3b\n");
  ASSERT_TRUE(listener3b.WaitUntilSatisfied());

  // Now verify about:blank.
  url = "about:blank";
  printf("***** %s\n", url.c_str());
  ExtensionTestMessageListener listener4a("content script: " + url, false);
  ExtensionTestMessageListener listener4b("execute: " + url, false);
  ui_test_utils::NavigateToURL(browser(), GURL(url));
  printf("***** Wait 4a\n");
  ASSERT_TRUE(listener4a.WaitUntilSatisfied());
  printf("***** Wait 4b\n");
  ASSERT_TRUE(listener4b.WaitUntilSatisfied());

  // Now verify we can script a regular http page.
  ASSERT_TRUE(test_server()->Start());
  GURL page_url = test_server()->GetURL(kAllUrlsTarget);
  printf("***** %s\n", page_url.spec().c_str());
  ExtensionTestMessageListener listener5a("content script: " + page_url.spec(),
                                          false);
  ExtensionTestMessageListener listener5b("execute: " + page_url.spec(), false);
  ui_test_utils::NavigateToURL(browser(), page_url);
  printf("***** Wait 5a\n");
  ASSERT_TRUE(listener5a.WaitUntilSatisfied());
  printf("***** Wait 5b\n");
  ASSERT_TRUE(listener5b.WaitUntilSatisfied());

  printf("***** DONE!\n");
}

// Test that an extension NOT whitelisted for scripting can ask for <all_urls>
// and run scripts on non-restricted all pages.
IN_PROC_BROWSER_TEST_F(AllUrlsApiTest, RegularExtensions) {
  // First load the two extension.
  FilePath extension_dir1 = test_data_dir_.AppendASCII("all_urls")
                                          .AppendASCII("content_script");
  FilePath extension_dir2 = test_data_dir_.AppendASCII("all_urls")
                                          .AppendASCII("execute_script");

  ExtensionsService* service = browser()->profile()->GetExtensionsService();
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
