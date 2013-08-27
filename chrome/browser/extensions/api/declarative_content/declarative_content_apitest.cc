// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/test_extension_dir.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "content/public/test/browser_test_utils.h"

namespace extensions {
namespace {

const char kDeclarativeContentManifest[] =
    "{\n"
    "  \"name\": \"Declarative Content apitest\",\n"
    "  \"version\": \"0.1\",\n"
    "  \"manifest_version\": 2,\n"
    "  \"description\": \n"
    "      \"end-to-end browser test for the declarative Content API\",\n"
    "  \"background\": {\n"
    "    \"scripts\": [\"background.js\"]\n"
    "  },\n"
    "  \"permissions\": [\n"
    "    \"declarativeContent\"\n"
    "  ],\n"
    "  \"page_action\": {}\n"
    "}\n";

class DeclarativeContentApiTest : public ExtensionApiTest {
 public:
  DeclarativeContentApiTest()
      // Set the channel to "trunk" since declarativeContent is restricted
      // to trunk.
      : current_channel_(chrome::VersionInfo::CHANNEL_UNKNOWN) {
  }
  virtual ~DeclarativeContentApiTest() {}

  extensions::ScopedCurrentChannel current_channel_;
  TestExtensionDir ext_dir_;
};

IN_PROC_BROWSER_TEST_F(DeclarativeContentApiTest, Overview) {
  ext_dir_.WriteManifest(kDeclarativeContentManifest);
  ext_dir_.WriteFile(
      FILE_PATH_LITERAL("background.js"),
      "var declarative = chrome.declarative;\n"
      "\n"
      "var PageStateMatcher = chrome.declarativeContent.PageStateMatcher;\n"
      "var ShowPageAction = chrome.declarativeContent.ShowPageAction;\n"
      "\n"
      "var rule0 = {\n"
      "  conditions: [new PageStateMatcher({pageUrl: {hostPrefix: "
      "\"test1\"}}),\n"
      "               new PageStateMatcher({css: "
      "[\"input[type='password']\"]})],\n"
      "  actions: [new ShowPageAction()]\n"
      "}\n"
      "\n"
      "var testEvent = chrome.declarativeContent.onPageChanged;\n"
      "\n"
      "testEvent.removeRules(undefined, function() {\n"
      "  testEvent.addRules([rule0], function() {\n"
      "    chrome.test.sendMessage(\"ready\", function(reply) {\n"
      "    })\n"
      "  });\n"
      "});\n");
  ExtensionTestMessageListener ready("ready", true);
  const Extension* extension = LoadExtension(ext_dir_.unpacked_path());
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
