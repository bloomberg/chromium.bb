// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/extensions/extension_installed_bubble.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "components/bubble/bubble_controller.h"
#include "components/bubble/bubble_ui.h"
#include "components/signin/core/browser/signin_manager.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#include "ui/base/ui_base_features.h"

using extensions::Manifest;
using ActionType = extensions::ExtensionBuilder::ActionType;

class ExtensionInstalledBubbleBrowserTest
    : public SupportsTestDialog<ExtensionBrowserTest> {
 public:
  ExtensionInstalledBubbleBrowserTest()
      : disable_animations_(&ToolbarActionsBar::disable_animations_for_testing_,
                            true) {}

  std::unique_ptr<ExtensionInstalledBubble> MakeBubble(
      const std::string& name,
      base::Optional<ActionType> type,
      Manifest::Location location = Manifest::INTERNAL,
      std::unique_ptr<base::DictionaryValue> extra_keys = nullptr);

  // DialogBrowserTest:
  void ShowDialog(const std::string& name) override;

  BubbleController* GetExtensionBubbleControllerFromManager(
      BubbleManager* manager) const {
    for (auto& controller : manager->controllers_) {
      if (controller->GetName() == "ExtensionInstalled")
        return controller.get();
    }
    return nullptr;
  }

  BubbleUi* GetBubbleUiFromManager(BubbleManager* manager) const {
    BubbleController* controller =
        GetExtensionBubbleControllerFromManager(manager);
    return controller ? controller->bubble_ui_.get() : nullptr;
  }

 private:
  base::AutoReset<bool> disable_animations_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstalledBubbleBrowserTest);
};

std::unique_ptr<ExtensionInstalledBubble>
ExtensionInstalledBubbleBrowserTest::MakeBubble(
    const std::string& name,
    base::Optional<ActionType> type,
    Manifest::Location location,
    std::unique_ptr<base::DictionaryValue> extra_keys) {
  const SkBitmap kEmptyBitmap;
  extensions::ExtensionBuilder builder(name);
  if (type)
    builder.SetAction(*type);
  builder.SetLocation(location);
  if (extra_keys)
    builder.MergeManifest(std::move(extra_keys));
  scoped_refptr<const extensions::Extension> extension = builder.Build();
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
  base::Optional<ActionType> type;
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
    auto bubble = MakeBubble("No action", base::nullopt);
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

// Tests if the BubbleController gets removed from the BubbleManager when
// the BubbleUi is closed.
IN_PROC_BROWSER_TEST_F(ExtensionInstalledBubbleBrowserTest, CloseBubbleUi) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kSecondaryUiMd);

  auto bubble = MakeBubble("No action", base::nullopt);
  BubbleManager* manager = browser()->GetBubbleManager();
  manager->ShowBubble(std::move(bubble));

  BubbleUi* bubble_ui = GetBubbleUiFromManager(manager);
  DCHECK(bubble_ui);
  bubble_ui->Close();

  // BubbleManager should no longer hold the controller.
  EXPECT_FALSE(GetExtensionBubbleControllerFromManager(manager));
}
