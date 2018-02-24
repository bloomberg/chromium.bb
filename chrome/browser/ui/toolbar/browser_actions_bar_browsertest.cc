// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/browser_actions_bar_browsertest.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/browser_action_test_util.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/common/pref_names.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "extensions/test/extension_test_message_listener.h"

namespace {

scoped_refptr<const extensions::Extension> CreateExtension(
    const std::string& name,
    bool has_browser_action) {
  extensions::DictionaryBuilder manifest;
  manifest.Set("name", name).
           Set("description", "an extension").
           Set("manifest_version", 2).
           Set("version", "1.0");
  if (has_browser_action)
    manifest.Set("browser_action", extensions::DictionaryBuilder().Build());
  return extensions::ExtensionBuilder()
      .SetManifest(manifest.Build())
      .SetID(crx_file::id_util::GenerateId(name))
      .Build();
}

}  // namespace

// BrowserActionsBarBrowserTest:

BrowserActionsBarBrowserTest::BrowserActionsBarBrowserTest()
    : toolbar_model_(nullptr) {
}

BrowserActionsBarBrowserTest::~BrowserActionsBarBrowserTest() {
}

void BrowserActionsBarBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  ExtensionBrowserTest::SetUpCommandLine(command_line);
  ToolbarActionsBar::disable_animations_for_testing_ = true;
}

void BrowserActionsBarBrowserTest::SetUpOnMainThread() {
  ExtensionBrowserTest::SetUpOnMainThread();
  browser_actions_bar_.reset(new BrowserActionTestUtil(browser()));
  toolbar_model_ = ToolbarActionsModel::Get(profile());
}

void BrowserActionsBarBrowserTest::TearDownOnMainThread() {
  ToolbarActionsBar::disable_animations_for_testing_ = false;
  ExtensionBrowserTest::TearDownOnMainThread();
}

void BrowserActionsBarBrowserTest::LoadExtensions() {
  // Create three extensions with browser actions.
  extension_a_ = CreateExtension("alpha", true);
  extension_b_ = CreateExtension("beta", true);
  extension_c_ = CreateExtension("gamma", true);

  const extensions::Extension* extensions[] =
      { extension_a(), extension_b(), extension_c() };
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile());
  // Add each, and verify that it is both correctly added to the extension
  // registry and to the browser actions container.
  for (size_t i = 0; i < arraysize(extensions); ++i) {
    extension_service()->AddExtension(extensions[i]);
    EXPECT_TRUE(registry->enabled_extensions().GetByID(extensions[i]->id())) <<
        extensions[i]->name();
    EXPECT_EQ(static_cast<int>(i + 1),
              browser_actions_bar_->NumberOfBrowserActions());
    EXPECT_TRUE(browser_actions_bar_->HasIcon(i));
    EXPECT_EQ(static_cast<int>(i + 1),
              browser_actions_bar()->VisibleBrowserActions());
  }
}

// Test the basic functionality.
IN_PROC_BROWSER_TEST_F(BrowserActionsBarBrowserTest, Basic) {
  // Load an extension with no browser action.
  extension_service()->AddExtension(CreateExtension("alpha", false).get());
  // This extension should be present in the model (it will receive a
  // synthesized action).
  EXPECT_EQ(1, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_TRUE(browser_actions_bar()->HasIcon(0));

  // Load an extension with a browser action; it will also be in the toolbar.
  extension_service()->AddExtension(CreateExtension("beta", true).get());
  EXPECT_EQ(2, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_TRUE(browser_actions_bar()->HasIcon(1));

  // Unload the extension; the icon should be removed.
  std::string id = browser_actions_bar()->GetExtensionId(1);
  UnloadExtension(id);
  EXPECT_EQ(1, browser_actions_bar()->NumberOfBrowserActions());
}

// Test moving various browser actions. This is not to check the logic of the
// move (that's in the toolbar model tests), but just to check our ui.
IN_PROC_BROWSER_TEST_F(BrowserActionsBarBrowserTest, MoveBrowserActions) {
  LoadExtensions();

  EXPECT_EQ(3, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(3, browser_actions_bar()->NumberOfBrowserActions());

  // Order is now A B C.
  EXPECT_EQ(extension_a()->id(), browser_actions_bar()->GetExtensionId(0));
  EXPECT_EQ(extension_b()->id(), browser_actions_bar()->GetExtensionId(1));
  EXPECT_EQ(extension_c()->id(), browser_actions_bar()->GetExtensionId(2));

  // Move C to first position. Order is C A B.
  toolbar_model()->MoveActionIcon(extension_c()->id(), 0);
  EXPECT_EQ(extension_c()->id(), browser_actions_bar()->GetExtensionId(0));
  EXPECT_EQ(extension_a()->id(), browser_actions_bar()->GetExtensionId(1));
  EXPECT_EQ(extension_b()->id(), browser_actions_bar()->GetExtensionId(2));

  // Move B to third position. Order is still C A B.
  toolbar_model()->MoveActionIcon(extension_b()->id(), 2);
  EXPECT_EQ(extension_c()->id(), browser_actions_bar()->GetExtensionId(0));
  EXPECT_EQ(extension_a()->id(), browser_actions_bar()->GetExtensionId(1));
  EXPECT_EQ(extension_b()->id(), browser_actions_bar()->GetExtensionId(2));

  // Move B to middle position. Order is C B A.
  toolbar_model()->MoveActionIcon(extension_b()->id(), 1);
  EXPECT_EQ(extension_c()->id(), browser_actions_bar()->GetExtensionId(0));
  EXPECT_EQ(extension_b()->id(), browser_actions_bar()->GetExtensionId(1));
  EXPECT_EQ(extension_a()->id(), browser_actions_bar()->GetExtensionId(2));
}

IN_PROC_BROWSER_TEST_F(BrowserActionsBarBrowserTest, Visibility) {
  LoadExtensions();

  // Change container to show only one action, rest in overflow: A, [B, C].
  toolbar_model()->SetVisibleIconCount(1);
  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());

  // Disable extension A (should disappear). State becomes: B [C].
  DisableExtension(extension_a()->id());
  EXPECT_EQ(2, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(extension_b()->id(), browser_actions_bar()->GetExtensionId(0));

  // Enable A again. A should get its spot in the same location and the bar
  // should not grow. For details: http://crbug.com/35349.
  // State becomes: A, [B, C].
  EnableExtension(extension_a()->id());
  EXPECT_EQ(3, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(extension_a()->id(), browser_actions_bar()->GetExtensionId(0));

  // Disable C (in overflow). State becomes: A, [B].
  DisableExtension(extension_c()->id());
  EXPECT_EQ(2, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(extension_a()->id(), browser_actions_bar()->GetExtensionId(0));

  // Enable C again. State becomes: A, [B, C].
  EnableExtension(extension_c()->id());
  EXPECT_EQ(3, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(extension_a()->id(), browser_actions_bar()->GetExtensionId(0));

  // Now we have 3 extensions. Make sure they are all visible. State: A, B, C.
  toolbar_model()->SetVisibleIconCount(3);
  EXPECT_EQ(3, browser_actions_bar()->VisibleBrowserActions());

  // Disable extension A (should disappear). State becomes: B, C.
  DisableExtension(extension_a()->id());
  EXPECT_EQ(2, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_EQ(2, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(extension_b()->id(), browser_actions_bar()->GetExtensionId(0));

  // Disable extension B (should disappear). State becomes: C.
  DisableExtension(extension_b()->id());
  EXPECT_EQ(1, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(extension_c()->id(), browser_actions_bar()->GetExtensionId(0));

  // Enable B. State becomes: B, C.
  EnableExtension(extension_b()->id());
  EXPECT_EQ(2, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_EQ(2, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(extension_b()->id(), browser_actions_bar()->GetExtensionId(0));

  // Enable A. State becomes: A, B, C.
  EnableExtension(extension_a()->id());
  EXPECT_EQ(3, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_EQ(3, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(extension_a()->id(), browser_actions_bar()->GetExtensionId(0));

  // Shrink the browser actions bar to zero visible icons.
  // No icons should be visible, but we *should* overflow and have a
  // non-empty size.
  toolbar_model()->SetVisibleIconCount(0);
  EXPECT_EQ(0, browser_actions_bar()->VisibleBrowserActions());
  ToolbarActionsBar* toolbar_actions_bar =
      browser_actions_bar()->GetToolbarActionsBar();
  EXPECT_TRUE(toolbar_actions_bar->NeedsOverflow());

  // Reset visibility count to 2. State should be A, B, [C], and we should
  // overflow.
  toolbar_model()->SetVisibleIconCount(2);
  EXPECT_EQ(2, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(extension_a()->id(), browser_actions_bar()->GetExtensionId(0));
  EXPECT_EQ(extension_b()->id(), browser_actions_bar()->GetExtensionId(1));
  EXPECT_TRUE(toolbar_actions_bar->NeedsOverflow());

  // Disable C (the overflowed extension). State should now be A, B, and we
  // should not overflow.
  DisableExtension(extension_c()->id());
  EXPECT_EQ(2, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(extension_a()->id(), browser_actions_bar()->GetExtensionId(0));
  EXPECT_EQ(extension_b()->id(), browser_actions_bar()->GetExtensionId(1));
  EXPECT_FALSE(toolbar_actions_bar->NeedsOverflow());

  // Re-enable C. We should still only have 2 visible icons, still with
  // overflow.
  EnableExtension(extension_c()->id());
  EXPECT_EQ(2, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(extension_a()->id(), browser_actions_bar()->GetExtensionId(0));
  EXPECT_EQ(extension_b()->id(), browser_actions_bar()->GetExtensionId(1));
  EXPECT_TRUE(toolbar_actions_bar->NeedsOverflow());
}

// Test that, with the toolbar action redesign, actions that want to run have
// the proper appearance.
IN_PROC_BROWSER_TEST_F(BrowserActionsBarBrowserTest,
                       TestUiForActionsWantToRun) {
  LoadExtensions();
  EXPECT_EQ(3, browser_actions_bar()->VisibleBrowserActions());

  // Load an extension with a page action.
  scoped_refptr<const extensions::Extension> page_action_extension =
      extensions::ExtensionBuilder("page action")
          .SetAction(extensions::ExtensionBuilder::ActionType::PAGE_ACTION)
          .SetLocation(extensions::Manifest::INTERNAL)
          .Build();
  extension_service()->AddExtension(page_action_extension.get());

  // Verify that the extension was added at the last index.
  EXPECT_EQ(4, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(page_action_extension->id(),
            browser_actions_bar()->GetExtensionId(3));
  EXPECT_FALSE(browser_actions_bar()->ActionButtonWantsToRun(3));

  // Make the extension want to run on the current page.
  ExtensionAction* action = extensions::ExtensionActionManager::Get(profile())->
      GetExtensionAction(*page_action_extension);
  ASSERT_TRUE(action);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  int tab_id = SessionTabHelper::IdForTab(web_contents);
  action->SetIsVisible(tab_id, true);
  extensions::ExtensionActionAPI* extension_action_api =
      extensions::ExtensionActionAPI::Get(profile());
  extension_action_api->NotifyChange(action, web_contents, profile());
  // Verify that the extension's button has the proper UI.
  EXPECT_TRUE(browser_actions_bar()->ActionButtonWantsToRun(3));

  // Make the extension not want to run, and check that the special UI goes
  // away.
  action->SetIsVisible(tab_id, false);
  extension_action_api->NotifyChange(action, web_contents, profile());
  EXPECT_FALSE(browser_actions_bar()->ActionButtonWantsToRun(3));

  // Reduce the visible icon count so that the extension is hidden.
  toolbar_model()->SetVisibleIconCount(3);

  // The extension should want to run whether or not it's hidden.
  action->SetIsVisible(tab_id, true);
  extension_action_api->NotifyChange(action, web_contents, profile());
  EXPECT_TRUE(browser_actions_bar()->ActionButtonWantsToRun(3));
}

IN_PROC_BROWSER_TEST_F(BrowserActionsBarBrowserTest,
                       BrowserActionPopupTest) {
  // Load up two extensions that have browser action popups.
  base::FilePath data_dir =
      test_data_dir_.AppendASCII("api_test").AppendASCII("browser_action");
  const extensions::Extension* first_extension =
      LoadExtension(data_dir.AppendASCII("open_popup"));
  ASSERT_TRUE(first_extension);
  const extensions::Extension* second_extension =
      LoadExtension(data_dir.AppendASCII("remove_popup"));
  ASSERT_TRUE(second_extension);

  // Verify state: two actions, in the order of [first, second].
  EXPECT_EQ(2, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(first_extension->id(), browser_actions_bar()->GetExtensionId(0));
  EXPECT_EQ(second_extension->id(), browser_actions_bar()->GetExtensionId(1));

  // Do a little piping to get at the underlying ExtensionActionViewControllers.
  ToolbarActionsBar* toolbar_actions_bar =
      browser_actions_bar()->GetToolbarActionsBar();
  const std::vector<ToolbarActionViewController*>& toolbar_actions =
      toolbar_actions_bar->GetActions();
  ASSERT_EQ(2u, toolbar_actions.size());
  EXPECT_EQ(first_extension->id(), toolbar_actions[0]->GetId());
  EXPECT_EQ(second_extension->id(), toolbar_actions[1]->GetId());
  ExtensionActionViewController* first_controller =
      static_cast<ExtensionActionViewController*>(toolbar_actions[0]);
  ExtensionActionViewController* second_controller =
      static_cast<ExtensionActionViewController*>(toolbar_actions[1]);

  // Neither should yet be showing a popup.
  EXPECT_FALSE(browser_actions_bar()->HasPopup());
  EXPECT_FALSE(first_controller->is_showing_popup());
  EXPECT_FALSE(second_controller->is_showing_popup());

  // Click on the first extension's browser action. This should open a popup.
  browser_actions_bar()->Press(0);
  EXPECT_TRUE(browser_actions_bar()->HasPopup());
  EXPECT_TRUE(first_controller->is_showing_popup());
  EXPECT_FALSE(second_controller->is_showing_popup());

  {
    content::WindowedNotificationObserver observer(
        extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED,
        content::NotificationService::AllSources());
    // Clicking on the second extension's browser action should open the
    // second's popup. Since we only allow one extension popup at a time, this
    // should also close the first popup.
    browser_actions_bar()->Press(1);
    // Closing an extension popup isn't always synchronous; wait for a
    // notification.
    observer.Wait();
    EXPECT_TRUE(browser_actions_bar()->HasPopup());
    EXPECT_FALSE(first_controller->is_showing_popup());
    EXPECT_TRUE(second_controller->is_showing_popup());
  }

  {
    // Clicking on the second extension's browser action a second time should
    // result in closing the popup.
    content::WindowedNotificationObserver observer(
        extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED,
        content::NotificationService::AllSources());
    browser_actions_bar()->Press(1);
    observer.Wait();
    EXPECT_FALSE(browser_actions_bar()->HasPopup());
    EXPECT_FALSE(first_controller->is_showing_popup());
    EXPECT_FALSE(second_controller->is_showing_popup());
  }
}

IN_PROC_BROWSER_TEST_F(BrowserActionsBarBrowserTest,
                       OverflowedBrowserActionPopupTest) {
  std::unique_ptr<BrowserActionTestUtil> overflow_bar =
      browser_actions_bar()->CreateOverflowBar();

  // Load up two extensions that have browser action popups.
  base::FilePath data_dir =
      test_data_dir_.AppendASCII("api_test").AppendASCII("browser_action");
  const extensions::Extension* first_extension =
      LoadExtension(data_dir.AppendASCII("open_popup"));
  ASSERT_TRUE(first_extension);
  const extensions::Extension* second_extension =
      LoadExtension(data_dir.AppendASCII("remove_popup"));
  ASSERT_TRUE(second_extension);

  // Verify state: two actions, in the order of [first, second].
  EXPECT_EQ(2, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(first_extension->id(), browser_actions_bar()->GetExtensionId(0));
  EXPECT_EQ(second_extension->id(), browser_actions_bar()->GetExtensionId(1));

  // Do a little piping to get at the underlying ExtensionActionViewControllers.
  ToolbarActionsBar* main_tab = browser_actions_bar()->GetToolbarActionsBar();
  std::vector<ToolbarActionViewController*> toolbar_actions =
      main_tab->GetActions();
  ASSERT_EQ(2u, toolbar_actions.size());
  EXPECT_EQ(first_extension->id(), toolbar_actions[0]->GetId());
  EXPECT_EQ(second_extension->id(), toolbar_actions[1]->GetId());
  ExtensionActionViewController* first_controller_main =
      static_cast<ExtensionActionViewController*>(toolbar_actions[0]);
  ExtensionActionViewController* second_controller_main =
      static_cast<ExtensionActionViewController*>(toolbar_actions[1]);

  ToolbarActionsBar* overflow_tab = overflow_bar->GetToolbarActionsBar();
  toolbar_actions = overflow_tab->GetActions();
  ExtensionActionViewController* second_controller_overflow =
      static_cast<ExtensionActionViewController*>(toolbar_actions[1]);

  toolbar_model()->SetVisibleIconCount(0);

  EXPECT_EQ(0, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(2, overflow_bar->VisibleBrowserActions());

  // Neither should yet be showing a popup.
  EXPECT_FALSE(browser_actions_bar()->HasPopup());
  EXPECT_FALSE(second_controller_main->is_showing_popup());
  EXPECT_FALSE(second_controller_overflow->is_showing_popup());

  // Click on the first extension's browser action. This should open a popup.
  overflow_bar->Press(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(browser_actions_bar()->HasPopup());
  EXPECT_FALSE(overflow_bar->HasPopup());
  EXPECT_TRUE(second_controller_main->is_showing_popup());
  EXPECT_FALSE(second_controller_overflow->is_showing_popup());

  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(1u, main_tab->GetIconCount());
  EXPECT_EQ(second_controller_main->GetId(),
            browser_actions_bar()->GetExtensionId(0));
  EXPECT_EQ(2, overflow_bar->VisibleBrowserActions());
  EXPECT_EQ(2u, overflow_tab->GetIconCount());
  EXPECT_EQ(first_controller_main->GetId(),
            overflow_bar->GetExtensionId(0));

  {
    content::WindowedNotificationObserver observer(
        extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED,
        content::NotificationService::AllSources());
    second_controller_main->HidePopup();
    observer.Wait();
  }

  EXPECT_FALSE(browser_actions_bar()->HasPopup());
  EXPECT_FALSE(overflow_bar->HasPopup());
  EXPECT_FALSE(second_controller_main->is_showing_popup());
  EXPECT_FALSE(second_controller_overflow->is_showing_popup());
  EXPECT_EQ(0, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(2, overflow_bar->VisibleBrowserActions());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(first_controller_main->GetId(),
            browser_actions_bar()->GetExtensionId(0));
  EXPECT_EQ(second_controller_main->GetId(),
            browser_actions_bar()->GetExtensionId(1));
}

// Test removing an extension that has an popup showing.
// Regression test for crbug.com/599467.
IN_PROC_BROWSER_TEST_F(BrowserActionsBarBrowserTest,
                       OverflowedBrowserActionPopupTestRemoval) {
  std::unique_ptr<BrowserActionTestUtil> overflow_bar =
      browser_actions_bar()->CreateOverflowBar();

  // Install an extension and shrink the visible count to zero so the extension
  // is overflowed.
  base::FilePath data_dir =
      test_data_dir_.AppendASCII("api_test").AppendASCII("browser_action");
  const extensions::Extension* extension =
      LoadExtension(data_dir.AppendASCII("open_popup"));
  ASSERT_TRUE(extension);
  toolbar_model()->SetVisibleIconCount(0);
  EXPECT_EQ(0, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(1, overflow_bar->VisibleBrowserActions());
  EXPECT_FALSE(browser_actions_bar()->HasPopup());

  // Click on the overflowed extension, causing it to pop out.
  overflow_bar->Press(0);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(browser_actions_bar()->HasPopup());
  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());

  {
    content::WindowedNotificationObserver observer(
        extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED,
        content::NotificationService::AllSources());
    // Remove the extension. Nothing should crash.
    extension_service()->UnloadExtension(
        extension->id(), extensions::UnloadedExtensionReason::UNINSTALL);
    observer.Wait();
  }

  EXPECT_EQ(0, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(0, overflow_bar->VisibleBrowserActions());
  EXPECT_EQ(0u, toolbar_model()->toolbar_items().size());
}

// Test that page action popups work with the toolbar redesign.
IN_PROC_BROWSER_TEST_F(BrowserActionsBarBrowserTest,
                       PageActionPopupsTest) {
  ExtensionTestMessageListener listener("ready", false);
  const extensions::Extension* page_action_extension =
      LoadExtension(test_data_dir_.AppendASCII("trigger_actions").
                        AppendASCII("page_action_popup"));
  ASSERT_TRUE(page_action_extension);
  listener.WaitUntilSatisfied();
  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(page_action_extension->id(),
            browser_actions_bar()->GetExtensionId(0));
  browser_actions_bar()->Press(0);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(browser_actions_bar()->HasPopup());
  // Cleanup the popup (to avoid having windows open at tear down).
  browser_actions_bar()->HidePopup();
  content::RunAllTasksUntilIdle();
  EXPECT_FALSE(browser_actions_bar()->HasPopup());
}

// Test removing an action while it is popped out.
IN_PROC_BROWSER_TEST_F(BrowserActionsBarBrowserTest, RemovePoppedOutAction) {
  // First, load up three separate extensions and reduce the visible count to
  // one (so that two are in the overflow).
  scoped_refptr<const extensions::Extension> extension1 =
      extensions::ExtensionBuilder("extension1")
          .SetAction(extensions::ExtensionBuilder::ActionType::BROWSER_ACTION)
          .SetLocation(extensions::Manifest::INTERNAL)
          .Build();
  extension_service()->AddExtension(extension1.get());
  scoped_refptr<const extensions::Extension> extension2 =
      extensions::ExtensionBuilder("extension2")
          .SetAction(extensions::ExtensionBuilder::ActionType::BROWSER_ACTION)
          .SetLocation(extensions::Manifest::INTERNAL)
          .Build();
  extension_service()->AddExtension(extension2.get());
  scoped_refptr<const extensions::Extension> extension3 =
      extensions::ExtensionBuilder("extension3")
          .SetAction(extensions::ExtensionBuilder::ActionType::BROWSER_ACTION)
          .SetLocation(extensions::Manifest::INTERNAL)
          .Build();
  extension_service()->AddExtension(extension3.get());

  toolbar_model()->SetVisibleIconCount(1);

  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(3, browser_actions_bar()->NumberOfBrowserActions());

  // Pop out Extension 3 (index 3).
  base::Closure closure = base::DoNothing();
  ToolbarActionsBar* toolbar_actions_bar =
      browser()->window()->GetToolbarActionsBar();
  EXPECT_EQ(extension3->id(), toolbar_actions_bar->GetActions()[2]->GetId());
  toolbar_actions_bar->PopOutAction(toolbar_actions_bar->GetActions()[2], false,
                                    closure);
  EXPECT_EQ(2, browser_actions_bar()->VisibleBrowserActions());
  ASSERT_TRUE(toolbar_actions_bar->popped_out_action());
  EXPECT_EQ(extension3->id(),
            toolbar_actions_bar->popped_out_action()->GetId());

  // Remove extension 2 - there should still be one left in the overflow
  // (extension 2) and one left on the main bar (extension 1).
  extension_service()->UnloadExtension(
      extension3->id(), extensions::UnloadedExtensionReason::DISABLE);
  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(2, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_FALSE(toolbar_actions_bar->popped_out_action());

  // Add back extension 3, and reduce visible size to 0.
  extension_service()->AddExtension(extension3.get());
  toolbar_model()->SetVisibleIconCount(0);
  EXPECT_EQ(0, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(3, browser_actions_bar()->NumberOfBrowserActions());

  // Pop out extension 2 (index 1).
  EXPECT_EQ(extension2->id(), toolbar_actions_bar->GetActions()[1]->GetId());
  toolbar_actions_bar->PopOutAction(toolbar_actions_bar->GetActions()[1], false,
                                    closure);
  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());
  ASSERT_TRUE(toolbar_actions_bar->popped_out_action());
  EXPECT_EQ(extension2->id(),
            toolbar_actions_bar->popped_out_action()->GetId());

  // Remove extension 2 - the remaining two should both be overflowed.
  extension_service()->UnloadExtension(
      extension2->id(), extensions::UnloadedExtensionReason::DISABLE);
  EXPECT_EQ(0, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(2, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_FALSE(toolbar_actions_bar->popped_out_action());

  // Finally, set visible count to 1, pop out extension 1, and remove it. There
  // should only be one action left on the bar.
  toolbar_model()->SetVisibleIconCount(1);
  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(extension3->id(), toolbar_actions_bar->GetActions()[1]->GetId());
  toolbar_actions_bar->PopOutAction(toolbar_actions_bar->GetActions()[1], false,
                                    closure);
  EXPECT_EQ(2, browser_actions_bar()->VisibleBrowserActions());
  ASSERT_TRUE(toolbar_actions_bar->popped_out_action());
  EXPECT_EQ(extension3->id(),
            toolbar_actions_bar->popped_out_action()->GetId());
  extension_service()->UnloadExtension(
      extension3->id(), extensions::UnloadedExtensionReason::DISABLE);
  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(1, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_FALSE(toolbar_actions_bar->popped_out_action());
}

// A test that runs in incognito mode.
class BrowserActionsBarIncognitoTest : public BrowserActionsBarBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    BrowserActionsBarBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("incognito");
  }
};

// Tests that first loading an extension action in an incognito profile, then
// removing the incognito profile and using the extension action in a normal
// profile doesn't crash.
// Regression test for crbug.com/663726.
IN_PROC_BROWSER_TEST_F(BrowserActionsBarIncognitoTest, IncognitoMode) {
  EXPECT_TRUE(browser()->profile()->IsOffTheRecord());
  const extensions::Extension* extension = LoadExtensionIncognito(
      test_data_dir_.AppendASCII("api_test/browser_action_with_icon"));
  ASSERT_TRUE(extension);
  Browser* second_browser =
      new Browser(Browser::CreateParams(profile()->GetOriginalProfile(), true));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(second_browser->profile()->IsOffTheRecord());

  content::WindowedNotificationObserver window_close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED, content::Source<Browser>(browser()));
  browser()->window()->Close();
  window_close_observer.Wait();

  std::vector<ToolbarActionViewController*> actions =
      second_browser->window()->GetToolbarActionsBar()->GetActions();
  ASSERT_EQ(1u, actions.size());
  gfx::Image icon = actions[0]->GetIcon(
      second_browser->tab_strip_model()->GetActiveWebContents(),
      gfx::Size(ToolbarActionsBar::IconWidth(false),
                ToolbarActionsBar::IconHeight()));
  const gfx::ImageSkia* skia = icon.ToImageSkia();
  ASSERT_TRUE(skia);
  // Force the image to try and load a representation.
  skia->GetRepresentation(2.0);
}
