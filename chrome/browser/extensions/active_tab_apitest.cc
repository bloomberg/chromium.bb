// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/logging.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
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

}  // namespace
}  // namespace extensions
