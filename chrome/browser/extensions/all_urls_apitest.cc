// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/ui_test_utils.h"

const std::string kAllUrlsTarget =
    "files/extensions/api_test/all_urls/index.html";

typedef ExtensionApiTest AllUrlsApiTest;

IN_PROC_BROWSER_TEST_F(AllUrlsApiTest, WhitelistedExtension) {
  Extension::emit_traces_for_whitelist_extension_test_ = true;

  // First load the two extension.
  FilePath extension_dir1 = test_data_dir_.AppendASCII("all_urls")
                                          .AppendASCII("content_script");
  FilePath extension_dir2 = test_data_dir_.AppendASCII("all_urls")
                                          .AppendASCII("execute_script");

  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  const size_t size_before = service->extensions()->size();
  std::cout << "***** LoadExtension1 called \n" << std::flush;
  ASSERT_TRUE(LoadExtension(extension_dir1));
  std::cout << "***** LoadExtension2 called \n" << std::flush;
  ASSERT_TRUE(LoadExtension(extension_dir2));
  std::cout << "***** LoadExtensions done \n" << std::flush;
  EXPECT_EQ(size_before + 2, service->extensions()->size());
  Extension* extensionA = service->extensions()->at(size_before);
  Extension* extensionB = service->extensions()->at(size_before + 1);

  // Then add the two extensions to the whitelist.
  Extension::ScriptingWhitelist whitelist;
  whitelist.push_back(extensionA->id().c_str());
  whitelist.push_back(extensionB->id().c_str());
  Extension::SetScriptingWhitelist(whitelist);

  // Ideally, we'd set the whitelist first and then load the extensions.
  // However, we can't reliably know the ids of the extensions until we load
  // them so we reload them so that the whitelist is in effect from the start.
  std::cout << "***** ReloadExtension1 called \n" << std::flush;
  ReloadExtension(extensionA->id());
  std::cout << "***** ReloadExtension2 called \n" << std::flush;
  ReloadExtension(extensionB->id());
  std::cout << "***** ReloadExtensions done \n" << std::flush;

  std::string url;

  // Now verify we run content scripts on chrome://newtab/.
  url = "chrome://newtab/";
  std::cout << "***** " << url.c_str() << "\n" << std::flush;
  ExtensionTestMessageListener listener1a("content script: " + url, false);
  ExtensionTestMessageListener listener1b("execute: " + url, false);
  ui_test_utils::NavigateToURL(browser(), GURL(url));
  std::cout << "***** Wait 1a\n" << std::flush;
  ASSERT_TRUE(listener1a.WaitUntilSatisfied());
  std::cout << "***** Wait 1b\n" << std::flush;
  ASSERT_TRUE(listener1b.WaitUntilSatisfied());

  // Now verify data: urls.
  url = "data:text/html;charset=utf-8,<html>asdf</html>";
  std::cout << "***** " << url.c_str() << "\n" << std::flush;
  ExtensionTestMessageListener listener2a("content script: " + url, false);
  ExtensionTestMessageListener listener2b("execute: " + url, false);
  ui_test_utils::NavigateToURL(browser(), GURL(url));
  std::cout << "***** Wait 2a\n" << std::flush;
  ASSERT_TRUE(listener2a.WaitUntilSatisfied());
  std::cout << "***** Wait 2b\n" << std::flush;
  ASSERT_TRUE(listener2b.WaitUntilSatisfied());

  // Now verify about:version.
  url = "about:version";
  std::cout << "***** " << url.c_str() << "\n" << std::flush;
  ExtensionTestMessageListener listener3a("content script: " + url, false);
  ExtensionTestMessageListener listener3b("execute: " + url, false);
  ui_test_utils::NavigateToURL(browser(), GURL(url));
  std::cout << "***** Wait 3a\n" << std::flush;
  ASSERT_TRUE(listener3a.WaitUntilSatisfied());
  std::cout << "***** Wait 3b\n" << std::flush;
  ASSERT_TRUE(listener3b.WaitUntilSatisfied());

  // Now verify about:blank.
  url = "about:blank";
  std::cout << "***** " << url.c_str() << "\n" << std::flush;
  ExtensionTestMessageListener listener4a("content script: " + url, false);
  ExtensionTestMessageListener listener4b("execute: " + url, false);
  ui_test_utils::NavigateToURL(browser(), GURL(url));
  std::cout << "***** Wait 4a\n" << std::flush;
  ASSERT_TRUE(listener4a.WaitUntilSatisfied());
  std::cout << "***** Wait 4b\n" << std::flush;
  ASSERT_TRUE(listener4b.WaitUntilSatisfied());

  // Now verify we can script a regular http page.
  ASSERT_TRUE(test_server()->Start());
  GURL page_url = test_server()->GetURL(kAllUrlsTarget);
  std::cout << "***** " << page_url.spec().c_str() << "\n" << std::flush;
  ExtensionTestMessageListener listener5a("content script: " + page_url.spec(),
                                          false);
  ExtensionTestMessageListener listener5b("execute: " + page_url.spec(), false);
  ui_test_utils::NavigateToURL(browser(), page_url);
  std::cout << "***** Wait 5a\n" << std::flush;
  ASSERT_TRUE(listener5a.WaitUntilSatisfied());
  std::cout << "***** Wait 5b\n" << std::flush;
  ASSERT_TRUE(listener5b.WaitUntilSatisfied());

  std::cout << "***** DONE!\n" << std::flush;
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
