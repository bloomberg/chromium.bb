// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"

using extensions::Extension;

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ScriptBadge) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("script_badge/basics")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;
  const ExtensionAction* script_badge = extension->script_badge();
  ASSERT_TRUE(script_badge);

  const int tab_id = browser()->GetActiveTabContents()->
      extension_tab_helper()->tab_id();
  EXPECT_EQ(GURL(extension->GetResourceURL("default_popup.html")),
            script_badge->GetPopupUrl(tab_id));

  {
    ResultCatcher catcher;
    // Tell the extension to update the script badge state.
    ui_test_utils::NavigateToURL(
        browser(), GURL(extension->GetResourceURL("update.html")));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  // Test that we received the changes.
  EXPECT_EQ(GURL(extension->GetResourceURL("set_popup.html")),
            script_badge->GetPopupUrl(tab_id));

  {
    // Simulate the script badge being clicked.
    ResultCatcher catcher;
    ExtensionService* service = browser()->profile()->GetExtensionService();
    service->browser_event_router()->ScriptBadgeExecuted(
        browser()->profile(), *script_badge, tab_id);
    EXPECT_TRUE(catcher.GetNextResult());
  }
}
