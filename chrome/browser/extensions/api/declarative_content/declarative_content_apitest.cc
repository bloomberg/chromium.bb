// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/features/feature.h"
#include "content/public/test/browser_test_utils.h"

namespace extensions {
namespace {

class DeclarativeContentApiTest : public ExtensionApiTest {
 public:
  DeclarativeContentApiTest()
      // Set the channel to "trunk" since declarativeContent is restricted
      // to trunk.
      : current_channel_(chrome::VersionInfo::CHANNEL_UNKNOWN) {
  }
  virtual ~DeclarativeContentApiTest() {}

  extensions::Feature::ScopedCurrentChannel current_channel_;
};

IN_PROC_BROWSER_TEST_F(DeclarativeContentApiTest, Overview) {
  ExtensionTestMessageListener ready("ready", true);
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("declarative_content/overview"));
  ASSERT_TRUE(extension);
  const ExtensionAction* page_action =
      ExtensionActionManager::Get(browser()->profile())->
      GetPageAction(*extension);
  ASSERT_TRUE(page_action);

  ASSERT_TRUE(ready.WaitUntilSatisfied());
  content::WebContents* const tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  const int tab_id = ExtensionTabUtil::GetTabId(tab);

  NavigateInRenderer(tab, GURL("http://test1/"));

  // The declarative API should show the page action instantly, rather
  // than waiting for the extension to run.
  EXPECT_TRUE(page_action->GetIsVisible(tab_id));

  // Make sure leaving a matching page unshows the page action.
  NavigateInRenderer(tab, GURL("http://not_checked/"));
  EXPECT_FALSE(page_action->GetIsVisible(tab_id));

  // Insert a password field to make sure that's noticed.
  ASSERT_TRUE(content::ExecuteScript(
      tab, "document.body.innerHTML = '<input type=\"password\">';"));
  // Give the mutation observer a chance to run and send back the
  // matching-selector update.
  ASSERT_TRUE(content::ExecuteScript(tab, std::string()));
  EXPECT_TRUE(page_action->GetIsVisible(tab_id));

  // Remove it again to make sure that reverts the action.
  ASSERT_TRUE(content::ExecuteScript(
      tab, "document.body.innerHTML = 'Hello world';"));
  // Give the mutation observer a chance to run and send back the
  // matching-selector update.
  ASSERT_TRUE(content::ExecuteScript(tab, std::string()));
  EXPECT_FALSE(page_action->GetIsVisible(tab_id));
}

}  // namespace
}  // namespace extensions
