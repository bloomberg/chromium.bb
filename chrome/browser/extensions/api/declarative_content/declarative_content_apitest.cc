// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/test_extension_dir.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

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
    "  \"page_action\": {},\n"
    "  \"permissions\": [\n"
    "    \"declarativeContent\"\n"
    "  ]\n"
    "}\n";

const char kBackgroundHelpers[] =
    "var PageStateMatcher = chrome.declarativeContent.PageStateMatcher;\n"
    "var ShowPageAction = chrome.declarativeContent.ShowPageAction;\n"
    "var onPageChanged = chrome.declarativeContent.onPageChanged;\n"
    "var Reply = window.domAutomationController.send.bind(\n"
    "    window.domAutomationController);\n"
    "\n"
    "function setRules(rules, responseString) {\n"
    "  onPageChanged.removeRules(undefined, function() {\n"
    "    onPageChanged.addRules(rules, function() {\n"
    "      if (chrome.runtime.lastError) {\n"
    "        Reply(chrome.runtime.lastError.message);\n"
    "        return;\n"
    "      }\n"
    "      Reply(responseString);\n"
    "    });\n"
    "  });\n"
    "};\n";

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
      "  conditions: [new PageStateMatcher({\n"
      "                   pageUrl: {hostPrefix: \"test1\"}}),\n"
      "               new PageStateMatcher({\n"
      "                   css: [\"input[type='password']\"]})],\n"
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
  // Notice that we touch offsetTop to force a synchronous layout.
  ASSERT_TRUE(content::ExecuteScript(
      tab, "document.body.innerHTML = '<input type=\"password\">';"
           "document.body.offsetTop;"));

  // Give the style match a chance to run and send back the matching-selector
  // update.  This takes one time through the Blink message loop to apply the
  // style to the new element, and a second to dedupe updates.
  // FIXME: Remove this after https://codereview.chromium.org/145663012/
  ASSERT_TRUE(content::ExecuteScript(tab, std::string()));
  ASSERT_TRUE(content::ExecuteScript(tab, std::string()));

  EXPECT_TRUE(page_action->GetIsVisible(tab_id))
      << "Adding a matching element should show the page action.";

  // Remove it again to make sure that reverts the action.
  // Notice that we touch offsetTop to force a synchronous layout.
  ASSERT_TRUE(content::ExecuteScript(
      tab, "document.body.innerHTML = 'Hello world';"
           "document.body.offsetTop;"));

  // Give the style match a chance to run and send back the matching-selector
  // update.  This takes one time through the Blink message loop to apply the
  // style to the new element, and a second to dedupe updates.
  // FIXME: Remove this after https://codereview.chromium.org/145663012/
  ASSERT_TRUE(content::ExecuteScript(tab, std::string()));
  ASSERT_TRUE(content::ExecuteScript(tab, std::string()));

  EXPECT_FALSE(page_action->GetIsVisible(tab_id))
      << "Removing the matching element should hide the page action again.";
}

// http://crbug.com/304373
IN_PROC_BROWSER_TEST_F(DeclarativeContentApiTest,
                       UninstallWhileActivePageAction) {
  ext_dir_.WriteManifest(kDeclarativeContentManifest);
  ext_dir_.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundHelpers);
  const Extension* extension = LoadExtension(ext_dir_.unpacked_path());
  ASSERT_TRUE(extension);
  const std::string extension_id = extension->id();
  const ExtensionAction* page_action = ExtensionActionManager::Get(
      browser()->profile())->GetPageAction(*extension);
  ASSERT_TRUE(page_action);

  const std::string kTestRule =
      "setRules([{\n"
      "  conditions: [new PageStateMatcher({\n"
      "                   pageUrl: {hostPrefix: \"test\"}})],\n"
      "  actions: [new ShowPageAction()]\n"
      "}], 'test_rule');\n";
  EXPECT_EQ("test_rule",
            ExecuteScriptInBackgroundPage(extension_id, kTestRule));

  content::WebContents* const tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  const int tab_id = ExtensionTabUtil::GetTabId(tab);

  NavigateInRenderer(tab, GURL("http://test/"));

  EXPECT_TRUE(page_action->GetIsVisible(tab_id));
  EXPECT_TRUE(WaitForPageActionVisibilityChangeTo(1));
  LocationBarTesting* location_bar =
      browser()->window()->GetLocationBar()->GetLocationBarForTesting();
  EXPECT_EQ(1, location_bar->PageActionCount());
  EXPECT_EQ(1, location_bar->PageActionVisibleCount());

  ReloadExtension(extension_id);  // Invalidates page_action and extension.
  EXPECT_EQ("test_rule",
            ExecuteScriptInBackgroundPage(extension_id, kTestRule));
  // TODO(jyasskin): Apply new rules to existing tabs, without waiting for a
  // navigation.
  NavigateInRenderer(tab, GURL("http://test/"));
  EXPECT_TRUE(WaitForPageActionVisibilityChangeTo(1));
  EXPECT_EQ(1, location_bar->PageActionCount());
  EXPECT_EQ(1, location_bar->PageActionVisibleCount());

  UnloadExtension(extension_id);
  NavigateInRenderer(tab, GURL("http://test/"));
  EXPECT_TRUE(WaitForPageActionVisibilityChangeTo(0));
  EXPECT_EQ(0, location_bar->PageActionCount());
  EXPECT_EQ(0, location_bar->PageActionVisibleCount());
}

// This tests against a renderer crash that was present during development.
IN_PROC_BROWSER_TEST_F(DeclarativeContentApiTest,
                       DISABLED_AddExtensionMatchingExistingTabWithDeadFrames) {
  ext_dir_.WriteManifest(kDeclarativeContentManifest);
  ext_dir_.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundHelpers);
  content::WebContents* const tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  const int tab_id = ExtensionTabUtil::GetTabId(tab);

  ASSERT_TRUE(content::ExecuteScript(
      tab, "document.body.innerHTML = '<iframe src=\"http://test2\">';"));
  // Replace the iframe to destroy its WebFrame.
  ASSERT_TRUE(content::ExecuteScript(
      tab, "document.body.innerHTML = '<span class=\"foo\">';"));

  const Extension* extension = LoadExtension(ext_dir_.unpacked_path());
  ASSERT_TRUE(extension);
  const ExtensionAction* page_action = ExtensionActionManager::Get(
      browser()->profile())->GetPageAction(*extension);
  ASSERT_TRUE(page_action);
  EXPECT_FALSE(page_action->GetIsVisible(tab_id));

  EXPECT_EQ("rule0",
            ExecuteScriptInBackgroundPage(
                extension->id(),
                "setRules([{\n"
                "  conditions: [new PageStateMatcher({\n"
                "                   css: [\"span[class=foo]\"]})],\n"
                "  actions: [new ShowPageAction()]\n"
                "}], 'rule0');\n"));
  // Give the renderer a chance to apply the rules change and notify the
  // browser.  This takes one time through the Blink message loop to receive
  // the rule change and apply the new stylesheet, and a second to dedupe the
  // update.
  ASSERT_TRUE(content::ExecuteScript(tab, std::string()));
  ASSERT_TRUE(content::ExecuteScript(tab, std::string()));

  EXPECT_FALSE(tab->IsCrashed());
  EXPECT_TRUE(page_action->GetIsVisible(tab_id))
      << "Loading an extension when an open page matches its rules "
      << "should show the page action.";

  EXPECT_EQ("removed",
            ExecuteScriptInBackgroundPage(
                extension->id(),
                "onPageChanged.removeRules(undefined, function() {\n"
                "  window.domAutomationController.send('removed');\n"
                "});\n"));
  EXPECT_FALSE(page_action->GetIsVisible(tab_id));
}

IN_PROC_BROWSER_TEST_F(DeclarativeContentApiTest,
                       ShowPageActionWithoutPageAction) {
  std::string manifest_without_page_action = kDeclarativeContentManifest;
  ReplaceSubstringsAfterOffset(
      &manifest_without_page_action, 0, "\"page_action\": {},", "");
  ext_dir_.WriteManifest(manifest_without_page_action);
  ext_dir_.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundHelpers);
  const Extension* extension = LoadExtension(ext_dir_.unpacked_path());
  ASSERT_TRUE(extension);

  EXPECT_THAT(ExecuteScriptInBackgroundPage(
                  extension->id(),
                  "setRules([{\n"
                  "  conditions: [new PageStateMatcher({\n"
                  "                   pageUrl: {hostPrefix: \"test\"}})],\n"
                  "  actions: [new ShowPageAction()]\n"
                  "}], 'test_rule');\n"),
              testing::HasSubstr("without a page action"));

  content::WebContents* const tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  NavigateInRenderer(tab, GURL("http://test/"));

  EXPECT_EQ(NULL,
            ExtensionActionManager::Get(browser()->profile())->
                GetPageAction(*extension));
  EXPECT_EQ(0,
            browser()->window()->GetLocationBar()->GetLocationBarForTesting()->
            PageActionCount());
}

IN_PROC_BROWSER_TEST_F(DeclarativeContentApiTest,
                       CanonicalizesPageStateMatcherCss) {
  ext_dir_.WriteManifest(kDeclarativeContentManifest);
  ext_dir_.WriteFile(
      FILE_PATH_LITERAL("background.js"),
      "var PageStateMatcher = chrome.declarativeContent.PageStateMatcher;\n"
      "function Return(obj) {\n"
      "  window.domAutomationController.send('' + obj);\n"
      "}\n");
  const Extension* extension = LoadExtension(ext_dir_.unpacked_path());
  ASSERT_TRUE(extension);

  EXPECT_EQ("input[type=\"password\"]",
            ExecuteScriptInBackgroundPage(
                extension->id(),
                "var psm = new PageStateMatcher(\n"
                "    {css: [\"input[type='password']\"]});\n"
                "Return(psm.css);"));

  EXPECT_THAT(ExecuteScriptInBackgroundPage(
                  extension->id(),
                  "try {\n"
                  "  new PageStateMatcher({css: 'Not-an-array'});\n"
                  "  Return('Failed to throw');\n"
                  "} catch (e) {\n"
                  "  Return(e.message);\n"
                  "}\n"),
              testing::ContainsRegex("css.*Expected 'array'"));
  EXPECT_THAT(ExecuteScriptInBackgroundPage(
                  extension->id(),
                  "try {\n"
                  "  new PageStateMatcher({css: [null]});\n"  // Not a string.
                  "  Return('Failed to throw');\n"
                  "} catch (e) {\n"
                  "  Return(e.message);\n"
                  "}\n"),
              testing::ContainsRegex("css\\.0.*Expected 'string'"));
  EXPECT_THAT(ExecuteScriptInBackgroundPage(
                  extension->id(),
                  "try {\n"
                  // Invalid CSS:
                  "  new PageStateMatcher({css: [\"input''\"]});\n"
                  "  Return('Failed to throw');\n"
                  "} catch (e) {\n"
                  "  Return(e.message);\n"
                  "}\n"),
              testing::ContainsRegex("valid.*: input''$"));
  EXPECT_THAT(ExecuteScriptInBackgroundPage(
                  extension->id(),
                  "try {\n"
                  // "Complex" selector:
                  "  new PageStateMatcher({css: ['div input']});\n"
                  "  Return('Failed to throw');\n"
                  "} catch (e) {\n"
                  "  Return(e.message);\n"
                  "}\n"),
              testing::ContainsRegex("compound selector.*: div input$"));
}

}  // namespace
}  // namespace extensions
