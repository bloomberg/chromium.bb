// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/active_tab_permission_granter.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/test/result_catcher.h"

using content::WebContents;

namespace extensions {

namespace {
// This extension ID is used for tests require a stable ID over multiple
// extension installs.
const char kId[] = "pgoakhfeplldmjheffidklpoklkppipp";

// Default keybinding to use for emulating user-defined shortcut overrides. The
// test extensions use Alt+Shift+F and Alt+Shift+H.
const char kAltShiftG[] = "Alt+Shift+G";
}

class CommandsApiTest : public ExtensionApiTest {
 public:
  CommandsApiTest() {}
  virtual ~CommandsApiTest() {}

 protected:
  BrowserActionTestUtil GetBrowserActionsBar() {
    return BrowserActionTestUtil(browser());
  }

  bool IsGrantedForTab(const Extension* extension,
                       const content::WebContents* web_contents) {
    return extension->permissions_data()->HasAPIPermissionForTab(
        SessionTabHelper::IdForTab(web_contents), APIPermission::kTab);
  }

#if defined(OS_CHROMEOS)
  void RunChromeOSConversionTest(const std::string& extension_path) {
    // Setup the environment.
    ASSERT_TRUE(test_server()->Start());
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
    ASSERT_TRUE(RunExtensionTest(extension_path)) << message_;
    ui_test_utils::NavigateToURL(
        browser(), test_server()->GetURL("files/extensions/test_file.txt"));

    ResultCatcher catcher;

    // Send all expected keys (Search+Shift+{Left, Up, Right, Down}).
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_LEFT, false, true, false, true));
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_UP, false, true, false, true));
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_RIGHT, false, true, false, true));
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_DOWN, false, true, false, true));

    ASSERT_TRUE(catcher.GetNextResult());
  }
#endif  // OS_CHROMEOS
};

// Test the basic functionality of the Keybinding API:
// - That pressing the shortcut keys should perform actions (activate the
//   browser action or send an event).
// - Note: Page action keybindings are tested in PageAction test below.
// - The shortcut keys taken by one extension are not overwritten by the last
//   installed extension.
IN_PROC_BROWSER_TEST_F(CommandsApiTest, Basic) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("keybinding/basics")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // Load this extension, which uses the same keybindings but sets the page
  // to different colors. This is so we can see that it doesn't interfere. We
  // don't test this extension in any other way (it should otherwise be
  // immaterial to this test).
  ASSERT_TRUE(RunExtensionTest("keybinding/conflicting")) << message_;

  // Test that there are two browser actions in the toolbar.
  ASSERT_EQ(2, GetBrowserActionsBar().NumberOfBrowserActions());

  ui_test_utils::NavigateToURL(browser(),
      test_server()->GetURL("files/extensions/test_file.txt"));

  // activeTab shouldn't have been granted yet.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  EXPECT_FALSE(IsGrantedForTab(extension, tab));

  // Activate the shortcut (Ctrl+Shift+F).
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, true, true, false, false));

  // activeTab should now be granted.
  EXPECT_TRUE(IsGrantedForTab(extension, tab));

  // Verify the command worked.
  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab,
      "setInterval(function(){"
      "  if(document.body.bgColor == 'red'){"
      "    window.domAutomationController.send(true)}}, 100)",
      &result));
  ASSERT_TRUE(result);

  // Activate the shortcut (Ctrl+Shift+Y).
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_Y, true, true, false, false));

  result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab,
      "setInterval(function(){"
      "  if(document.body.bgColor == 'blue'){"
      "    window.domAutomationController.send(true)}}, 100)",
      &result));
  ASSERT_TRUE(result);
}

// Flaky on linux and chromeos, http://crbug.com/165825
#if defined(OS_MACOSX) || defined(OS_WIN)
#define MAYBE_PageAction PageAction
#else
#define MAYBE_PageAction DISABLED_PageAction
#endif
IN_PROC_BROWSER_TEST_F(CommandsApiTest, MAYBE_PageAction) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("keybinding/page_action")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  {
    // Load a page, the extension will detect the navigation and request to show
    // the page action icon.
    ResultCatcher catcher;
    ui_test_utils::NavigateToURL(browser(),
        test_server()->GetURL("files/extensions/test_file.txt"));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  // Make sure it appears and is the right one.
  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(1));
  int tab_id = SessionTabHelper::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents())->session_id().id();
  ExtensionAction* action =
      ExtensionActionManager::Get(browser()->profile())->
      GetPageAction(*extension);
  ASSERT_TRUE(action);
  EXPECT_EQ("Make this page red", action->GetTitle(tab_id));

  // Activate the shortcut (Alt+Shift+F).
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, false, true, true, false));

  // Verify the command worked (the page action turns the page red).
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab,
      "setInterval(function(){"
      "  if(document.body.bgColor == 'red'){"
      "    window.domAutomationController.send(true)}}, 100)",
      &result));
  ASSERT_TRUE(result);
}

// This test validates that the getAll query API function returns registered
// commands as well as synthesized ones and that inactive commands (like the
// synthesized ones are in nature) have no shortcuts.
IN_PROC_BROWSER_TEST_F(CommandsApiTest, SynthesizedCommand) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("keybinding/synthesized")) << message_;
}

// This test validates that an extension cannot request a shortcut that is
// already in use by Chrome.
IN_PROC_BROWSER_TEST_F(CommandsApiTest, DontOverwriteSystemShortcuts) {
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ASSERT_TRUE(RunExtensionTest("keybinding/dont_overwrite_system")) << message_;

  ui_test_utils::NavigateToURL(browser(),
      test_server()->GetURL("files/extensions/test_file.txt"));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  // Activate the shortcut (Alt+Shift+F) to make the page blue.
  {
    ResultCatcher catcher;
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_F, false, true, true, false));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab,
      "setInterval(function() {"
      "  if (document.body.bgColor == 'blue') {"
      "    window.domAutomationController.send(true)}}, 100)",
      &result));
  ASSERT_TRUE(result);

  // Activate the bookmark shortcut (Ctrl+D) to make the page green (should not
  // work without requesting via chrome_settings_overrides).
#if defined(OS_MACOSX)
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_D, false, false, false, true));
#else
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_D, true, false, false, false));
#endif

  // The page should still be blue.
  result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab,
      "setInterval(function() {"
      "  if (document.body.bgColor == 'blue') {"
      "    window.domAutomationController.send(true)}}, 100)",
      &result));
  ASSERT_TRUE(result);

  // Activate the shortcut (Ctrl+F) to make the page red (should not work).
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, true, false, false, false));

  // The page should still be blue.
  result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab,
      "setInterval(function() {"
      "  if (document.body.bgColor == 'blue') {"
      "    window.domAutomationController.send(true)}}, 100)",
      &result));
  ASSERT_TRUE(result);
}

// This test validates that an extension can override the Chrome bookmark
// shortcut if it has requested to do so.
IN_PROC_BROWSER_TEST_F(CommandsApiTest, OverwriteBookmarkShortcut) {
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // This functionality requires a feature flag.
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "--enable-override-bookmarks-ui",
      "1");

  ASSERT_TRUE(RunExtensionTest("keybinding/overwrite_bookmark_shortcut"))
      << message_;

  ui_test_utils::NavigateToURL(browser(),
      test_server()->GetURL("files/extensions/test_file.txt"));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  // Activate the shortcut (Ctrl+D) to make the page green.
  {
    ResultCatcher catcher;
#if defined(OS_MACOSX)
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_D, false, false, false, true));
#else
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_D, true, false, false, false));
#endif
    ASSERT_TRUE(catcher.GetNextResult());
  }

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab,
      "setInterval(function() {"
      "  if (document.body.bgColor == 'green') {"
      "    window.domAutomationController.send(true)}}, 100)",
      &result));
  ASSERT_TRUE(result);
}

// This test validates that an extension override of the Chrome bookmark
// shortcut does not supersede the same keybinding by web pages.
IN_PROC_BROWSER_TEST_F(CommandsApiTest,
                       OverwriteBookmarkShortcutDoesNotOverrideWebKeybinding) {
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // This functionality requires a feature flag.
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "--enable-override-bookmarks-ui",
      "1");

  ASSERT_TRUE(RunExtensionTest("keybinding/overwrite_bookmark_shortcut"))
      << message_;

  ui_test_utils::NavigateToURL(browser(),
      test_server()->GetURL(
          "files/extensions/test_file_with_ctrl-d_keybinding.html"));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  // Activate the shortcut (Ctrl+D) which should be handled by the page and make
  // the background color magenta.
#if defined(OS_MACOSX)
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_D, false, false, false, true));
#else
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_D, true, false, false, false));
#endif

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab,
      "setInterval(function() {"
      "  if (document.body.bgColor == 'magenta') {"
      "    window.domAutomationController.send(true)}}, 100)",
      &result));
  ASSERT_TRUE(result);
}

// This test validates that user-set override of the Chrome bookmark shortcut in
// an extension that does not request it does supersede the same keybinding by
// web pages.
IN_PROC_BROWSER_TEST_F(CommandsApiTest,
                       OverwriteBookmarkShortcutByUserOverridesWebKeybinding) {
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // This functionality requires a feature flag.
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "--enable-override-bookmarks-ui",
      "1");

  ASSERT_TRUE(RunExtensionTest("keybinding/basics"))
      << message_;

  CommandService* command_service = CommandService::Get(browser()->profile());

  const Extension* extension = GetSingleLoadedExtension();
  // Simulate the user setting the keybinding to Ctrl+D.
#if defined(OS_MACOSX)
  const char* hotkey = "Command+D";
#else
  const char* hotkey = "Ctrl+D";
#endif  // defined(OS_MACOSX)
  command_service->UpdateKeybindingPrefs(
      extension->id(), manifest_values::kBrowserActionCommandEvent, hotkey);

  ui_test_utils::NavigateToURL(browser(),
      test_server()->GetURL(
          "files/extensions/test_file_with_ctrl-d_keybinding.html"));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  // Activate the shortcut (Ctrl+D) which should be handled by the extension and
  // make the background color red.
#if defined(OS_MACOSX)
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_D, false, false, false, true));
#else
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_D, true, false, false, false));
#endif

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab,
      "setInterval(function() {"
      "  if (document.body.bgColor == 'red') {"
      "    window.domAutomationController.send(true)}}, 100)",
      &result));
  ASSERT_TRUE(result);
}

#if defined(OS_WIN)
// Currently this feature is implemented on Windows only.
#define MAYBE_AllowDuplicatedMediaKeys AllowDuplicatedMediaKeys
#else
#define MAYBE_AllowDuplicatedMediaKeys DISABLED_AllowDuplicatedMediaKeys
#endif

// Test that media keys go to all extensions that register for them.
IN_PROC_BROWSER_TEST_F(CommandsApiTest, MAYBE_AllowDuplicatedMediaKeys) {
  ResultCatcher catcher;
  ASSERT_TRUE(RunExtensionTest("keybinding/non_global_media_keys_0"))
      << message_;
  ASSERT_TRUE(catcher.GetNextResult());
  ASSERT_TRUE(RunExtensionTest("keybinding/non_global_media_keys_1"))
      << message_;
  ASSERT_TRUE(catcher.GetNextResult());

  // Activate the Media Stop key.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_MEDIA_STOP, false, false, false, false));

  // We should get two success result.
  ASSERT_TRUE(catcher.GetNextResult());
  ASSERT_TRUE(catcher.GetNextResult());
}

IN_PROC_BROWSER_TEST_F(CommandsApiTest, ShortcutAddedOnUpdate) {
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath pem_path = test_data_dir_.
      AppendASCII("keybinding").AppendASCII("keybinding.pem");
  base::FilePath path_v1_unassigned = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding").AppendASCII("update")
                    .AppendASCII("v1_unassigned"),
      scoped_temp_dir.path().AppendASCII("v1_unassigned.crx"),
      pem_path,
      base::FilePath());
  base::FilePath path_v2 = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding").AppendASCII("update")
                    .AppendASCII("v2"),
      scoped_temp_dir.path().AppendASCII("v2.crx"),
      pem_path,
      base::FilePath());

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser()->profile());
  CommandService* command_service = CommandService::Get(browser()->profile());

  // Install v1 of the extension without keybinding assigned.
  ASSERT_TRUE(InstallExtension(path_v1_unassigned, 1));
  EXPECT_TRUE(registry->GetExtensionById(kId, ExtensionRegistry::ENABLED) !=
              NULL);

  // Verify it is set to nothing.
  ui::Accelerator accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_UNKNOWN, accelerator.key_code());

  // Update to version 2 with keybinding.
  EXPECT_TRUE(UpdateExtension(kId, path_v2, 0));
  EXPECT_TRUE(registry->GetExtensionById(kId, ExtensionRegistry::ENABLED) !=
              NULL);

  // Verify it has a command of Alt+Shift+F.
  accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_F, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());
}

IN_PROC_BROWSER_TEST_F(CommandsApiTest, ShortcutChangedOnUpdate) {
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath pem_path = test_data_dir_.
      AppendASCII("keybinding").AppendASCII("keybinding.pem");
  base::FilePath path_v1 = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding").AppendASCII("update")
                    .AppendASCII("v1"),
      scoped_temp_dir.path().AppendASCII("v1.crx"),
      pem_path,
      base::FilePath());
  base::FilePath path_v2_reassigned = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding").AppendASCII("update")
                    .AppendASCII("v2_reassigned"),
      scoped_temp_dir.path().AppendASCII("v2_reassigned.crx"),
      pem_path,
      base::FilePath());

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser()->profile());
  CommandService* command_service = CommandService::Get(browser()->profile());

  // Install v1 of the extension.
  ASSERT_TRUE(InstallExtension(path_v1, 1));
  EXPECT_TRUE(registry->GetExtensionById(kId, ExtensionRegistry::ENABLED) !=
              NULL);

  // Verify it has a command of Alt+Shift+F.
  ui::Accelerator accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_F, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());

  // Update to version 2 with different keybinding assigned.
  EXPECT_TRUE(UpdateExtension(kId, path_v2_reassigned, 0));
  EXPECT_TRUE(registry->GetExtensionById(kId, ExtensionRegistry::ENABLED) !=
              NULL);

  // Verify it has a command of Alt+Shift+H.
  accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_H, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());
}

IN_PROC_BROWSER_TEST_F(CommandsApiTest, ShortcutRemovedOnUpdate) {
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath pem_path = test_data_dir_.
      AppendASCII("keybinding").AppendASCII("keybinding.pem");
  base::FilePath path_v1 = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding").AppendASCII("update")
                    .AppendASCII("v1"),
      scoped_temp_dir.path().AppendASCII("v1.crx"),
      pem_path,
      base::FilePath());
  base::FilePath path_v2_unassigned = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding").AppendASCII("update")
                    .AppendASCII("v2_unassigned"),
      scoped_temp_dir.path().AppendASCII("v2_unassigned.crx"),
      pem_path,
      base::FilePath());

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser()->profile());
  CommandService* command_service = CommandService::Get(browser()->profile());

  // Install v1 of the extension.
  ASSERT_TRUE(InstallExtension(path_v1, 1));
  EXPECT_TRUE(registry->GetExtensionById(kId, ExtensionRegistry::ENABLED) !=
              NULL);

  // Verify it has a command of Alt+Shift+F.
  ui::Accelerator accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_F, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());

  // Update to version 2 without keybinding assigned.
  EXPECT_TRUE(UpdateExtension(kId, path_v2_unassigned, 0));
  EXPECT_TRUE(registry->GetExtensionById(kId, ExtensionRegistry::ENABLED) !=
              NULL);

  // Verify the keybinding gets set to nothing.
  accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_UNKNOWN, accelerator.key_code());
}

IN_PROC_BROWSER_TEST_F(CommandsApiTest,
                       ShortcutAddedOnUpdateAfterBeingAssignedByUser) {
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath pem_path = test_data_dir_.
      AppendASCII("keybinding").AppendASCII("keybinding.pem");
  base::FilePath path_v1_unassigned = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding").AppendASCII("update")
                    .AppendASCII("v1_unassigned"),
      scoped_temp_dir.path().AppendASCII("v1_unassigned.crx"),
      pem_path,
      base::FilePath());
  base::FilePath path_v2 = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding").AppendASCII("update")
                    .AppendASCII("v2"),
      scoped_temp_dir.path().AppendASCII("v2.crx"),
      pem_path,
      base::FilePath());

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser()->profile());
  CommandService* command_service = CommandService::Get(browser()->profile());

  // Install v1 of the extension without keybinding assigned.
  ASSERT_TRUE(InstallExtension(path_v1_unassigned, 1));
  EXPECT_TRUE(registry->GetExtensionById(kId, ExtensionRegistry::ENABLED) !=
              NULL);

  // Verify it is set to nothing.
  ui::Accelerator accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_UNKNOWN, accelerator.key_code());

  // Simulate the user setting the keybinding to Alt+Shift+G.
  command_service->UpdateKeybindingPrefs(
      kId, manifest_values::kBrowserActionCommandEvent, kAltShiftG);

  // Update to version 2 with keybinding.
  EXPECT_TRUE(UpdateExtension(kId, path_v2, 0));
  EXPECT_TRUE(registry->GetExtensionById(kId, ExtensionRegistry::ENABLED) !=
              NULL);

  // Verify the previously-set keybinding is still set.
  accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_G, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());
}

IN_PROC_BROWSER_TEST_F(CommandsApiTest,
                       ShortcutChangedOnUpdateAfterBeingReassignedByUser) {
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath pem_path = test_data_dir_.
      AppendASCII("keybinding").AppendASCII("keybinding.pem");
  base::FilePath path_v1 = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding").AppendASCII("update")
                    .AppendASCII("v1"),
      scoped_temp_dir.path().AppendASCII("v1.crx"),
      pem_path,
      base::FilePath());
  base::FilePath path_v2_reassigned = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding").AppendASCII("update")
                    .AppendASCII("v2_reassigned"),
      scoped_temp_dir.path().AppendASCII("v2_reassigned.crx"),
      pem_path,
      base::FilePath());

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser()->profile());
  CommandService* command_service = CommandService::Get(browser()->profile());

  // Install v1 of the extension.
  ASSERT_TRUE(InstallExtension(path_v1, 1));
  EXPECT_TRUE(registry->GetExtensionById(kId, ExtensionRegistry::ENABLED) !=
              NULL);

  // Verify it has a command of Alt+Shift+F.
  ui::Accelerator accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_F, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());

  // Simulate the user setting the keybinding to Alt+Shift+G.
  command_service->UpdateKeybindingPrefs(
      kId, manifest_values::kBrowserActionCommandEvent, kAltShiftG);

  // Update to version 2 with different keybinding assigned.
  EXPECT_TRUE(UpdateExtension(kId, path_v2_reassigned, 0));
  EXPECT_TRUE(registry->GetExtensionById(kId, ExtensionRegistry::ENABLED) !=
              NULL);

  // Verify it has a command of Alt+Shift+G.
  accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_G, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());
}

IN_PROC_BROWSER_TEST_F(CommandsApiTest,
                       ShortcutRemovedOnUpdateAfterBeingReassignedByUser) {
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath pem_path = test_data_dir_.
      AppendASCII("keybinding").AppendASCII("keybinding.pem");
  base::FilePath path_v1 = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding").AppendASCII("update")
                    .AppendASCII("v1"),
      scoped_temp_dir.path().AppendASCII("v1.crx"),
      pem_path,
      base::FilePath());
  base::FilePath path_v2_unassigned = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding").AppendASCII("update")
                    .AppendASCII("v2_unassigned"),
      scoped_temp_dir.path().AppendASCII("v2_unassigned.crx"),
      pem_path,
      base::FilePath());

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser()->profile());
  CommandService* command_service = CommandService::Get(browser()->profile());

  // Install v1 of the extension.
  ASSERT_TRUE(InstallExtension(path_v1, 1));
  EXPECT_TRUE(registry->GetExtensionById(kId, ExtensionRegistry::ENABLED) !=
              NULL);

  // Verify it has a command of Alt+Shift+F.
  ui::Accelerator accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_F, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());

  // Simulate the user reassigning the keybinding to Alt+Shift+G.
  command_service->UpdateKeybindingPrefs(
      kId, manifest_values::kBrowserActionCommandEvent, kAltShiftG);

  // Update to version 2 without keybinding assigned.
  EXPECT_TRUE(UpdateExtension(kId, path_v2_unassigned, 0));
  EXPECT_TRUE(registry->GetExtensionById(kId, ExtensionRegistry::ENABLED) !=
              NULL);

  // Verify the keybinding is still set.
  accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_G, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());
}

//
#if defined(OS_CHROMEOS) && !defined(NDEBUG)
// TODO(dtseng): Test times out on Chrome OS debug. See http://crbug.com/412456.
#define MAYBE_ContinuePropagation DISABLED_ContinuePropagation
#else
#define MAYBE_ContinuePropagation ContinuePropagation
#endif

IN_PROC_BROWSER_TEST_F(CommandsApiTest, MAYBE_ContinuePropagation) {
  // Setup the environment.
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(RunExtensionTest("keybinding/continue_propagation")) << message_;
  ui_test_utils::NavigateToURL(
      browser(), test_server()->GetURL("files/extensions/test_file.txt"));

  ResultCatcher catcher;

  // Activate the shortcut (Ctrl+Shift+F). The page should capture the
  // keystroke and not the extension since |onCommand| has no event listener
  // initially.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, true, true, false, false));
  ASSERT_TRUE(catcher.GetNextResult());

  // Now, the extension should have added an |onCommand| event listener.
  // Send the same key, but the |onCommand| listener should now receive it.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, true, true, false, false));
  ASSERT_TRUE(catcher.GetNextResult());

  // The extension should now have removed its |onCommand| event listener.
  // Finally, the page should again receive the key.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, true, true, false, false));
  ASSERT_TRUE(catcher.GetNextResult());
}

// Test is only applicable on Chrome OS.
#if defined(OS_CHROMEOS)
// http://crbug.com/410534
#if defined(USE_OZONE)
#define MAYBE_ChromeOSConversions DISABLED_ChromeOSConversions
#else
#define MAYBE_ChromeOSConversions ChromeOSConversions
#endif
IN_PROC_BROWSER_TEST_F(CommandsApiTest, MAYBE_ChromeOSConversions) {
  RunChromeOSConversionTest("keybinding/chromeos_conversions");
}
#endif  // OS_CHROMEOS

}  // namespace extensions
