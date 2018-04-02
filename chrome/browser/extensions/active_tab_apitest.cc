// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/logging.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/base/filename_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/extensions/extension_tab_util_delegate_chromeos.h"
#include "chromeos/login/scoped_test_public_session_login_state.h"
#endif

namespace extensions {
namespace {

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ActiveTab) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("active_tab"));
  ASSERT_TRUE(extension);

  // Shouldn't be initially granted based on activeTab.
  {
    ResultCatcher catcher;
    ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL(
            "/extensions/api_test/active_tab/page.html"));
    EXPECT_TRUE(catcher.GetNextResult()) << message_;
  }

  // Do one pass of BrowserAction without granting activeTab permission,
  // extension shouldn't have access to tab.url.
  {
    ResultCatcher catcher;
    ExtensionActionRunner::GetForWebContents(
        browser()->tab_strip_model()->GetActiveWebContents())
        ->RunAction(extension, false);
    EXPECT_TRUE(catcher.GetNextResult()) << message_;
  }

  // Granting to the extension should give it access to page.html.
  {
    ResultCatcher catcher;
    ExtensionActionRunner::GetForWebContents(
        browser()->tab_strip_model()->GetActiveWebContents())
        ->RunAction(extension, true);
    EXPECT_TRUE(catcher.GetNextResult()) << message_;
  }

#if defined(OS_CHROMEOS)
  // For the third pass grant the activeTab permission and do it in a public
  // session. URL should be scrubbed down to origin.
  {
    // Setup state.
    chromeos::ScopedTestPublicSessionLoginState login_state;
    auto delegate = std::make_unique<ExtensionTabUtilDelegateChromeOS>();
    ExtensionTabUtil::SetPlatformDelegate(delegate.get());

    ExtensionTestMessageListener listener(false);
    ResultCatcher catcher;
    ExtensionActionRunner::GetForWebContents(
        browser()->tab_strip_model()->GetActiveWebContents())
        ->RunAction(extension, true);
    EXPECT_TRUE(catcher.GetNextResult()) << message_;
    EXPECT_EQ(GURL(listener.message()).GetOrigin().spec(), listener.message());

    // Clean up.
    ExtensionTabUtil::SetPlatformDelegate(nullptr);
  }
#endif

  // Changing page should go back to it not having access.
  {
    ResultCatcher catcher;
    ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL(
            "/extensions/api_test/active_tab/final_page.html"));
    EXPECT_TRUE(catcher.GetNextResult()) << message_;
  }
}

// Tests the behavior of activeTab and its relation to an extension's ability to
// xhr file urls.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, XHRFileURLs) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  ExtensionTestMessageListener background_page_ready("ready",
                                                     false /*will_reply*/);
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("active_tab_file_urls"));
  ASSERT_TRUE(extension);
  const std::string extension_id = extension->id();

  // Ensure the extension's background page is ready.
  EXPECT_TRUE(background_page_ready.WaitUntilSatisfied());

  auto can_xhr_file_urls = [this, &extension_id]() {
    constexpr char script[] = R"(
      var req = new XMLHttpRequest();
      var url = '%s';
      req.open('GET', url, true);
      req.onload = function() {
        if (req.responseText === 'Hello!')
          window.domAutomationController.send('true');
      };

      // We track 'onloadend' to detect failures instead of 'onerror', since for
      // access check violations 'abort' event may be raised (instead of the
      // 'error' event).
      req.onloadend = function() {
        if (req.status === 0)
          window.domAutomationController.send('false');
      };
      req.send();
    )";

    base::FilePath test_file =
        test_data_dir_.DirName().AppendASCII("test_file.txt");
    std::string result = ExecuteScriptInBackgroundPage(
        extension_id,
        base::StringPrintf(script,
                           net::FilePathToFileURL(test_file).spec().c_str()));

    EXPECT_TRUE(result == "true" || result == "false");
    return result == "true";
  };

  // By default the extension should have file access enabled. However, since it
  // does not have host permissions to the localhost on the file scheme, it
  // should not be able to xhr file urls.
  EXPECT_TRUE(util::AllowFileAccess(extension_id, profile()));
  EXPECT_FALSE(can_xhr_file_urls());

  // Navigate to a file url (the extension's manifest.json in this case).
  GURL manifest_file_url =
      net::FilePathToFileURL(extension->path().AppendASCII("manifest.json"));
  ui_test_utils::NavigateToURL(browser(), manifest_file_url);

  // First don't grant the tab permission. Verify that the extension can't xhr
  // file urls.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ExtensionActionRunner::GetForWebContents(web_contents)
      ->RunAction(extension, false /*grant_tab_permissions*/);
  EXPECT_FALSE(can_xhr_file_urls());

  // Now grant the tab permission. Ensure the extension can now xhr file urls.
  ExtensionActionRunner::GetForWebContents(web_contents)
      ->RunAction(extension, true /*grant_tab_permissions*/);
  EXPECT_TRUE(can_xhr_file_urls());

  // Revoke extension's access to file urls. This will cause the extension to
  // reload, invalidating the |extension| pointer. Re-initialize the |extension|
  // pointer.
  background_page_ready.Reset();
  util::SetAllowFileAccess(extension_id, profile(), false /*allow*/);
  EXPECT_FALSE(util::AllowFileAccess(extension_id, profile()));
  extension = TestExtensionRegistryObserver(ExtensionRegistry::Get(profile()))
                  .WaitForExtensionLoaded();
  EXPECT_TRUE(extension);

  // Ensure the extension's background page is ready.
  EXPECT_TRUE(background_page_ready.WaitUntilSatisfied());

  // Grant the tab permission for the active url to the extension. Ensure it
  // still can't xhr file urls (since it does not have file access).
  ExtensionActionRunner::GetForWebContents(web_contents)
      ->RunAction(extension, true /*grant_tab_permissions*/);
  EXPECT_FALSE(can_xhr_file_urls());
}

}  // namespace
}  // namespace extensions
