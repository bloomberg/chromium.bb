// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_message_bubble_browsertest.h"

#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/extensions/extension_message_bubble_factory.h"
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
}

void ExtensionMessageBubbleBrowserTest::TearDownOnMainThread() {
  ExtensionMessageBubbleFactory::set_override_for_tests(
      ExtensionMessageBubbleFactory::NO_OVERRIDE);
  BrowserActionsBarBrowserTest::TearDownOnMainThread();
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
