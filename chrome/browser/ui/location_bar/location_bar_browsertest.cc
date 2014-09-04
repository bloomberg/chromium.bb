// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/test_extension_dir.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"

namespace {

const char kBackgroundScriptSource[] =
    "chrome.pageAction.onClicked.addListener(function() {\n"
    "  chrome.test.sendMessage('clicked');\n"
    "});\n"
    "chrome.test.sendMessage('registered');\n";
const char kManifestSource[] =
      "{"
      " \"name\": \"%s\","
      " \"version\": \"1.0\","
      " \"manifest_version\": 2,"
      " \"background\": { \"scripts\": [\"background.js\"] },"
      " \"page_action\": { }"
      "}";

}  // namespace

class LocationBarBrowserTest : public ExtensionBrowserTest {
 public:
  virtual ~LocationBarBrowserTest() {}

 protected:
  // Load an extension with a PageAction that sends a message when clicked.
  const extensions::Extension* LoadPageActionExtension(
      extensions::TestExtensionDir* dir);
};

const extensions::Extension* LocationBarBrowserTest::LoadPageActionExtension(
    extensions::TestExtensionDir* dir) {
  DCHECK(dir);

  dir->WriteManifest(base::StringPrintf(kManifestSource, "page_action1"));
  dir->WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundScriptSource);

  ExtensionTestMessageListener registered_listener("registered", false);
  const extensions::Extension* extension = LoadExtension(dir->unpacked_path());
  registered_listener.WaitUntilSatisfied();

  return extension;
}

// Test that page actions show up properly in the location bar. Since the
// page action logic is more fully tested as part of the extensions system, this
// only needs to check that they are displayed and clicking on them triggers
// the action.
// TODO(devlin): This flakily times out on Mac. crbug.com/410866
#if defined(OS_MACOSX)
#define MAYBE_PageActionUITest DISABLED_PageActionUITeset
#else
#define MAYBE_PageActionUITest PageActionUITest
#endif
IN_PROC_BROWSER_TEST_F(LocationBarBrowserTest, MAYBE_PageActionUITest) {
  LocationBarTesting* location_bar =
      browser()->window()->GetLocationBar()->GetLocationBarForTesting();

  // At the start, no page actions should exist.
  EXPECT_EQ(0, location_bar->PageActionCount());
  EXPECT_EQ(0, location_bar->PageActionVisibleCount());

  // Load two extensions with page actions.
  extensions::TestExtensionDir test_dir1;
  const extensions::Extension* page_action1 =
      LoadPageActionExtension(&test_dir1);
  ASSERT_TRUE(page_action1);

  extensions::TestExtensionDir test_dir2;
  const extensions::Extension* page_action2 =
      LoadPageActionExtension(&test_dir2);
  ASSERT_TRUE(page_action2);

  // Now there should be two page actions, but neither should be visible.
  EXPECT_EQ(2, location_bar->PageActionCount());
  EXPECT_EQ(0, location_bar->PageActionVisibleCount());

  // Make the first page action visible.
  ExtensionAction* action = extensions::ExtensionActionManager::Get(
                                profile())->GetPageAction(*page_action1);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  int tab_id = SessionTabHelper::IdForTab(tab);
  action->SetIsVisible(tab_id, true);
  extensions::ExtensionActionAPI::Get(profile())->NotifyChange(
      action, tab, profile());

  // Verify that only one action is visible and that it's the proper one.
  EXPECT_EQ(2, location_bar->PageActionCount());
  EXPECT_EQ(1, location_bar->PageActionVisibleCount());
  EXPECT_EQ(action, location_bar->GetVisiblePageAction(0u));

  // Trigger the visible page action, and ensure it executes.
  ExtensionTestMessageListener clicked_listener("clicked", false);
  clicked_listener.set_extension_id(page_action1->id());
  location_bar->TestPageActionPressed(0u);
  EXPECT_TRUE(clicked_listener.WaitUntilSatisfied());
}

class LocationBarBrowserTestWithRedesign : public LocationBarBrowserTest {
 private:
  virtual void SetUpCommandLine(base::CommandLine* command_line) OVERRIDE;

  scoped_ptr<extensions::FeatureSwitch::ScopedOverride> enable_redesign_;
};

void LocationBarBrowserTestWithRedesign::SetUpCommandLine(
    base::CommandLine* command_line) {
  LocationBarBrowserTest::SetUpCommandLine(command_line);
  enable_redesign_.reset(new extensions::FeatureSwitch::ScopedOverride(
      extensions::FeatureSwitch::extension_action_redesign(), true));
}

// Test that page actions are not displayed in the location bar if the
// extension action redesign switch is enabled.
IN_PROC_BROWSER_TEST_F(LocationBarBrowserTestWithRedesign,
                       PageActionUITestWithRedesign) {
  LocationBarTesting* location_bar =
      browser()->window()->GetLocationBar()->GetLocationBarForTesting();
  EXPECT_EQ(0, location_bar->PageActionCount());
  EXPECT_EQ(0, location_bar->PageActionVisibleCount());

  // Load an extension with a page action.
  extensions::TestExtensionDir test_dir1;
  const extensions::Extension* page_action1 =
      LoadPageActionExtension(&test_dir1);
  ASSERT_TRUE(page_action1);

  // We should still have no page actions.
  EXPECT_EQ(0, location_bar->PageActionCount());
  EXPECT_EQ(0, location_bar->PageActionVisibleCount());

  // Set the page action to be visible.
  ExtensionAction* action = extensions::ExtensionActionManager::Get(
                                profile())->GetPageAction(*page_action1);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  int tab_id = SessionTabHelper::IdForTab(tab);
  action->SetIsVisible(tab_id, true);
  extensions::ExtensionActionAPI::Get(profile())->NotifyChange(
      action, tab, profile());

  // We should still have no page actions.
  EXPECT_EQ(0, location_bar->PageActionCount());
  EXPECT_EQ(0, location_bar->PageActionVisibleCount());
}
