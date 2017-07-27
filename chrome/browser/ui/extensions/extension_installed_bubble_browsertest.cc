// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/extensions/extension_installed_bubble.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "components/signin/core/browser/signin_manager.h"
#include "extensions/common/manifest_constants.h"

using extensions::extension_action_test_util::ActionType;
using extensions::Manifest;

class ExtensionInstalledBubbleBrowserTest
    : public SupportsTestDialog<ExtensionBrowserTest> {
 public:
  ExtensionInstalledBubbleBrowserTest()
      : disable_animations_(&ToolbarActionsBar::disable_animations_for_testing_,
                            true) {}

  std::unique_ptr<ExtensionInstalledBubble> MakeBubble(
      const std::string& name,
      ActionType type,
      Manifest::Location location = Manifest::INTERNAL,
      std::unique_ptr<base::DictionaryValue> extra_keys = nullptr);

  // DialogBrowserTest:
  void ShowDialog(const std::string& name) override;

 private:
  base::AutoReset<bool> disable_animations_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstalledBubbleBrowserTest);
};

std::unique_ptr<ExtensionInstalledBubble>
ExtensionInstalledBubbleBrowserTest::MakeBubble(
    const std::string& name,
    ActionType type,
    Manifest::Location location,
    std::unique_ptr<base::DictionaryValue> extra_keys) {
  const SkBitmap kEmptyBitmap;
  scoped_refptr<const extensions::Extension> extension =
      extensions::extension_action_test_util::CreateActionExtension(
          name, type, location, std::move(extra_keys));
  extension_service()->AddExtension(extension.get());
  auto bubble = base::MakeUnique<ExtensionInstalledBubble>(
      extension.get(), browser(), SkBitmap());
  bubble->Initialize();
  return bubble;
}

void ExtensionInstalledBubbleBrowserTest::ShowDialog(const std::string& name) {
  // Default to Manifest::COMPONENT to test all anchoring locations. Without
  // this, a page action is added automatically, which will always be the
  // preferred anchor.
  Manifest::Location location = Manifest::COMPONENT;
  ActionType type = ActionType::NO_ACTION;
  if (name == "BrowserAction")
    type = ActionType::BROWSER_ACTION;
  else if (name == "PageAction")
    type = ActionType::PAGE_ACTION;

  // Use INTERNAL for these so that the instruction text and signin promo are
  // not suppressed.
  if (name == "SignInPromo" || name == "NoAction")
    location = Manifest::INTERNAL;

  auto extra_keys = base::MakeUnique<base::DictionaryValue>();
  if (name == "Omnibox")
    extra_keys->SetString(extensions::manifest_keys::kOmniboxKeyword, "foo");

  auto bubble = MakeBubble(name, type, location, std::move(extra_keys));
  BubbleManager* manager = browser()->GetBubbleManager();
  manager->ShowBubble(std::move(bubble));
}

IN_PROC_BROWSER_TEST_F(ExtensionInstalledBubbleBrowserTest,
                       InvokeDialog_BrowserAction) {
  RunDialog();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstalledBubbleBrowserTest,
                       InvokeDialog_PageAction) {
  RunDialog();
}

// Test anchoring to the app menu.
IN_PROC_BROWSER_TEST_F(ExtensionInstalledBubbleBrowserTest,
                       InvokeDialog_InstalledByDefault) {
  RunDialog();
}

// Test anchoring to the omnibox.
IN_PROC_BROWSER_TEST_F(ExtensionInstalledBubbleBrowserTest,
                       InvokeDialog_Omnibox) {
  RunDialog();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstalledBubbleBrowserTest,
                       InvokeDialog_SignInPromo) {
  RunDialog();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstalledBubbleBrowserTest,
                       InvokeDialog_NoAction) {
  // Sign in to supppress the signin promo.
  SigninManagerFactory::GetForProfile(browser()->profile())
      ->SetAuthenticatedAccountInfo("test", "test@example.com");
  RunDialog();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstalledBubbleBrowserTest,
                       DoNotShowHowToUseForSynthesizedActions) {
  {
    auto bubble = MakeBubble("No action", ActionType::NO_ACTION);
    EXPECT_EQ(0, bubble->options() & ExtensionInstalledBubble::HOW_TO_USE);
  }
  {
    auto bubble = MakeBubble("Browser action", ActionType::BROWSER_ACTION);
    EXPECT_NE(0, bubble->options() & ExtensionInstalledBubble::HOW_TO_USE);
  }
  {
    auto bubble = MakeBubble("Page action", ActionType::PAGE_ACTION);
    EXPECT_NE(0, bubble->options() & ExtensionInstalledBubble::HOW_TO_USE);
  }
}
