// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_message_bubble_browsertest.h"

#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_dir.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_message_bubble_factory.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/common/pref_names.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "extensions/common/feature_switch.h"
#include "extensions/test/extension_test_message_listener.h"

ExtensionMessageBubbleBrowserTest::ExtensionMessageBubbleBrowserTest() {
}

ExtensionMessageBubbleBrowserTest::~ExtensionMessageBubbleBrowserTest() {
}

void ExtensionMessageBubbleBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  BrowserActionsBarBrowserTest::SetUpCommandLine(command_line);
  // The dev mode warning bubble is an easy one to trigger, so we use that for
  // our testing purposes.
  dev_mode_bubble_override_.reset(
      new extensions::FeatureSwitch::ScopedOverride(
          extensions::FeatureSwitch::force_dev_mode_highlighting(),
          true));
  ExtensionMessageBubbleFactory::set_override_for_tests(
      ExtensionMessageBubbleFactory::OVERRIDE_ENABLED);
  ToolbarActionsBar::set_extension_bubble_appearance_wait_time_for_testing(0);
}

void ExtensionMessageBubbleBrowserTest::TearDownOnMainThread() {
  ExtensionMessageBubbleFactory::set_override_for_tests(
      ExtensionMessageBubbleFactory::NO_OVERRIDE);
  BrowserActionsBarBrowserTest::TearDownOnMainThread();
}

void ExtensionMessageBubbleBrowserTest::AddSettingsOverrideExtension(
    const std::string& settings_override_value) {
  DCHECK(!custom_extension_dir_);
  custom_extension_dir_.reset(new extensions::TestExtensionDir());
  std::string manifest = base::StringPrintf(
    "{\n"
    "  'name': 'settings override',\n"
    "  'version': '0.1',\n"
    "  'manifest_version': 2,\n"
    "  'description': 'controls settings',\n"
    "  'chrome_settings_overrides': {\n"
    "    %s\n"
    "  }\n"
    "}", settings_override_value.c_str());
  custom_extension_dir_->WriteManifestWithSingleQuotes(manifest);
  ASSERT_TRUE(LoadExtension(custom_extension_dir_->unpacked_path()));
}

void ExtensionMessageBubbleBrowserTest::TestBubbleAnchoredToExtensionAction() {
  scoped_refptr<const extensions::Extension> action_extension =
      extensions::extension_action_test_util::CreateActionExtension(
          "action_extension",
          extensions::extension_action_test_util::BROWSER_ACTION,
          extensions::Manifest::UNPACKED);
  extension_service()->AddExtension(action_extension.get());

  Browser* second_browser = new Browser(Browser::CreateParams(profile()));
  base::RunLoop().RunUntilIdle();

  CheckBubble(second_browser, ANCHOR_BROWSER_ACTION);
  CloseBubble(second_browser);
}

void ExtensionMessageBubbleBrowserTest::TestBubbleAnchoredToAppMenu() {
  scoped_refptr<const extensions::Extension> no_action_extension =
      extensions::extension_action_test_util::CreateActionExtension(
          "no_action_extension",
          extensions::extension_action_test_util::NO_ACTION,
          extensions::Manifest::UNPACKED);
  extension_service()->AddExtension(no_action_extension.get());
  Browser* second_browser = new Browser(Browser::CreateParams(profile()));
  base::RunLoop().RunUntilIdle();
  CheckBubble(second_browser, ANCHOR_APP_MENU);
  CloseBubble(second_browser);
}

void ExtensionMessageBubbleBrowserTest::
    TestBubbleAnchoredToAppMenuWithOtherAction() {
  scoped_refptr<const extensions::Extension> no_action_extension =
      extensions::extension_action_test_util::CreateActionExtension(
          "no_action_extension",
          extensions::extension_action_test_util::NO_ACTION,
          extensions::Manifest::UNPACKED);
  extension_service()->AddExtension(no_action_extension.get());

  scoped_refptr<const extensions::Extension> action_extension =
      extensions::extension_action_test_util::CreateActionExtension(
          "action_extension",
          extensions::extension_action_test_util::BROWSER_ACTION,
          extensions::Manifest::INTERNAL);
  extension_service()->AddExtension(action_extension.get());

  Browser* second_browser = new Browser(Browser::CreateParams(profile()));
  base::RunLoop().RunUntilIdle();

  CheckBubble(second_browser, ANCHOR_APP_MENU);
  CloseBubble(second_browser);
}

void ExtensionMessageBubbleBrowserTest::TestUninstallDangerousExtension() {
  // Load an extension that overrides the proxy setting.
  ExtensionTestMessageListener listener("registered", false);
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                        .AppendASCII("proxy")
                        .AppendASCII("register"));
  // Wait for it to complete.
  listener.WaitUntilSatisfied();

  // Create a second browser with the extension installed - the bubble will be
  // set to show.
  Browser* second_browser = new Browser(Browser::CreateParams(profile()));
  ASSERT_TRUE(second_browser);
  // Uninstall the extension before the bubble is shown. This should not crash,
  // and the bubble shouldn't be shown.
  extension_service()->UninstallExtension(
      extension->id(), extensions::UNINSTALL_REASON_FOR_TESTING,
      base::Bind(&base::DoNothing), nullptr);
  base::RunLoop().RunUntilIdle();
  CheckBubbleIsNotPresent(second_browser);
}

void ExtensionMessageBubbleBrowserTest::PreBubbleShowsOnStartup() {
  LoadExtension(test_data_dir_.AppendASCII("good_unpacked"));
}

void ExtensionMessageBubbleBrowserTest::TestBubbleShowsOnStartup() {
  base::RunLoop().RunUntilIdle();
  CheckBubble(browser(), ANCHOR_BROWSER_ACTION);
  CloseBubble(browser());
}

void ExtensionMessageBubbleBrowserTest::TestDevModeBubbleIsntShownTwice() {
  scoped_refptr<const extensions::Extension> action_extension =
      extensions::extension_action_test_util::CreateActionExtension(
          "action_extension",
          extensions::extension_action_test_util::BROWSER_ACTION,
          extensions::Manifest::UNPACKED);
  extension_service()->AddExtension(action_extension.get());

  Browser* second_browser = new Browser(Browser::CreateParams(profile()));
  base::RunLoop().RunUntilIdle();

  CheckBubble(second_browser, ANCHOR_BROWSER_ACTION);
  CloseBubble(second_browser);
  base::RunLoop().RunUntilIdle();

  // The bubble was already shown, so it shouldn't be shown again.
  Browser* third_browser = new Browser(Browser::CreateParams(profile()));
  base::RunLoop().RunUntilIdle();
  CheckBubbleIsNotPresent(third_browser);
}

void ExtensionMessageBubbleBrowserTest::TestControlledNewTabPageBubbleShown() {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("api_test")
                                          .AppendASCII("override")
                                          .AppendASCII("newtab")));
  CheckBubbleIsNotPresent(browser());
  chrome::NewTab(browser());
  base::RunLoop().RunUntilIdle();
  CheckBubble(browser(), ANCHOR_BROWSER_ACTION);
  CloseBubble(browser());
}

void ExtensionMessageBubbleBrowserTest::TestControlledHomeBubbleShown() {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton, true);

  const char kHomePage[] = "'homepage': 'https://www.google.com'\n";
  AddSettingsOverrideExtension(kHomePage);

  CheckBubbleIsNotPresent(browser());

  chrome::ExecuteCommandWithDisposition(browser(),
                                        IDC_HOME, NEW_FOREGROUND_TAB);
  base::RunLoop().RunUntilIdle();

  CheckBubble(browser(), ANCHOR_BROWSER_ACTION);
  CloseBubble(browser());
}

void ExtensionMessageBubbleBrowserTest::TestControlledSearchBubbleShown() {
  const char kSearchProvider[] =
      "'search_provider': {\n"
      "  'search_url': 'https://www.google.com/search?q={searchTerms}',\n"
      "  'is_default': true,\n"
      "  'favicon_url': 'https://www.google.com/favicon.icon',\n"
      "  'keyword': 'TheGoogs',\n"
      "  'name': 'Google',\n"
      "  'encoding': 'UTF-8'\n"
      "}\n";
  AddSettingsOverrideExtension(kSearchProvider);

  CheckBubbleIsNotPresent(browser());

  OmniboxView* omnibox =
      browser()->window()->GetLocationBar()->GetOmniboxView();
  omnibox->OnBeforePossibleChange();
  omnibox->SetUserText(base::ASCIIToUTF16("search for this"));
  omnibox->OnAfterPossibleChange(true);
  omnibox->model()->AcceptInput(CURRENT_TAB, false);
  base::RunLoop().RunUntilIdle();

  CheckBubble(browser(), ANCHOR_BROWSER_ACTION);
  CloseBubble(browser());
}
