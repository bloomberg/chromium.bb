// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/ui_test_utils.h"

const std::string kAllUrlsTarget =
    "files/extensions/api_test/all_urls/index.html";

typedef ExtensionApiTest AllUrlsApiTest;

namespace {

void Checkpoint(const char* message, const base::TimeTicks& start_time) {
  LOG(INFO) << message << " : "
            << (base::TimeTicks::Now() - start_time).InMilliseconds()
            << " ms" << std::flush;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(AllUrlsApiTest, WhitelistedExtension) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  Checkpoint("Setting up extensions", start_time);

  // First setup the two extensions.
  FilePath extension_dir1 = test_data_dir_.AppendASCII("all_urls")
                                          .AppendASCII("content_script");
  FilePath extension_dir2 = test_data_dir_.AppendASCII("all_urls")
                                          .AppendASCII("execute_script");

  // Then add the two extensions to the whitelist.
  Checkpoint("Setting up whitelist", start_time);
  Extension::ScriptingWhitelist whitelist;
  whitelist.push_back(Extension::GenerateIdForPath(extension_dir1));
  whitelist.push_back(Extension::GenerateIdForPath(extension_dir2));
  Extension::SetScriptingWhitelist(whitelist);

  // Then load extensions.
  Checkpoint("LoadExtension1", start_time);
  ExtensionService* service = browser()->profile()->GetExtensionService();
  const size_t size_before = service->extensions()->size();
  ASSERT_TRUE(LoadExtension(extension_dir1));
  Checkpoint("LoadExtension2", start_time);
  ASSERT_TRUE(LoadExtension(extension_dir2));
  EXPECT_EQ(size_before + 2, service->extensions()->size());

  std::string url;

  // Now verify we run content scripts on chrome://newtab/.
  Checkpoint("Verify content scripts", start_time);
  url = "chrome://newtab/";
  ExtensionTestMessageListener listener1a("content script: " + url, false);
  ExtensionTestMessageListener listener1b("execute: " + url, false);
  ui_test_utils::NavigateToURL(browser(), GURL(url));
  Checkpoint("Wait for 1a", start_time);
  ASSERT_TRUE(listener1a.WaitUntilSatisfied());
  Checkpoint("Wait for 1b", start_time);
  ASSERT_TRUE(listener1b.WaitUntilSatisfied());

  // Now verify data: urls.
  Checkpoint("Verify data:urls", start_time);
  url = "data:text/html;charset=utf-8,<html>asdf</html>";
  ExtensionTestMessageListener listener2a("content script: " + url, false);
  ExtensionTestMessageListener listener2b("execute: " + url, false);
  ui_test_utils::NavigateToURL(browser(), GURL(url));
  Checkpoint("Wait for 2a", start_time);
  ASSERT_TRUE(listener2a.WaitUntilSatisfied());
  Checkpoint("Wait for 2b", start_time);
  ASSERT_TRUE(listener2b.WaitUntilSatisfied());

  // Now verify about:version.
  Checkpoint("Verify about:version", start_time);
  url = "about:version";
  ExtensionTestMessageListener listener3a("content script: " + url, false);
  ExtensionTestMessageListener listener3b("execute: " + url, false);
  ui_test_utils::NavigateToURL(browser(), GURL(url));
  Checkpoint("Wait for 3a", start_time);
  ASSERT_TRUE(listener3a.WaitUntilSatisfied());
  Checkpoint("Wait for 3b", start_time);
  ASSERT_TRUE(listener3b.WaitUntilSatisfied());

  // Now verify about:blank.
  Checkpoint("Verify about:blank", start_time);
  url = "about:blank";
  ExtensionTestMessageListener listener4a("content script: " + url, false);
  ExtensionTestMessageListener listener4b("execute: " + url, false);
  ui_test_utils::NavigateToURL(browser(), GURL(url));
  Checkpoint("Wait for 4a", start_time);
  ASSERT_TRUE(listener4a.WaitUntilSatisfied());
  Checkpoint("Wait for 4b", start_time);
  ASSERT_TRUE(listener4b.WaitUntilSatisfied());

  // Now verify we can script a regular http page.
  Checkpoint("Verify regular http page", start_time);
  ASSERT_TRUE(test_server()->Start());
  GURL page_url = test_server()->GetURL(kAllUrlsTarget);
  ExtensionTestMessageListener listener5a("content script: " + page_url.spec(),
                                          false);
  ExtensionTestMessageListener listener5b("execute: " + page_url.spec(), false);
  ui_test_utils::NavigateToURL(browser(), page_url);
  Checkpoint("Wait for 5a", start_time);
  ASSERT_TRUE(listener5a.WaitUntilSatisfied());
  Checkpoint("Wait for 5ba", start_time);
  ASSERT_TRUE(listener5b.WaitUntilSatisfied());

  Checkpoint("Test complete", start_time);
}

// Test that an extension NOT whitelisted for scripting can ask for <all_urls>
// and run scripts on non-restricted all pages.
// Disabled, http://crbug.com/64304.
IN_PROC_BROWSER_TEST_F(AllUrlsApiTest, DISABLED_RegularExtensions) {
  // First load the two extension.
  FilePath extension_dir1 = test_data_dir_.AppendASCII("all_urls")
                                          .AppendASCII("content_script");
  FilePath extension_dir2 = test_data_dir_.AppendASCII("all_urls")
                                          .AppendASCII("execute_script");

  ExtensionService* service = browser()->profile()->GetExtensionService();
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
