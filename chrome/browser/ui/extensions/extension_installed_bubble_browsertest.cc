// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/extensions/extension_installed_bubble.h"
#include "extensions/common/feature_switch.h"

using ExtensionInstalledBubbleBrowserTest = ExtensionBrowserTest;

IN_PROC_BROWSER_TEST_F(ExtensionInstalledBubbleBrowserTest,
                       DoNotShowHowToUseForSynthesizedActions) {
  extensions::FeatureSwitch::ScopedOverride enable_redesign(
      extensions::FeatureSwitch::extension_action_redesign(),
      true);
  const SkBitmap kEmptyBitmap;
  {
    scoped_refptr<const extensions::Extension> extension =
        extensions::extension_action_test_util::CreateActionExtension(
            "No action", extensions::extension_action_test_util::NO_ACTION);
    extension_service()->AddExtension(extension.get());
    ExtensionInstalledBubble bubble(extension.get(), browser(), kEmptyBitmap);
    bubble.Initialize();
    EXPECT_EQ(0, bubble.options() & ExtensionInstalledBubble::HOW_TO_USE);
  }
  {
    scoped_refptr<const extensions::Extension> extension =
        extensions::extension_action_test_util::CreateActionExtension(
            "Browser action",
            extensions::extension_action_test_util::BROWSER_ACTION);
    extension_service()->AddExtension(extension.get());
    ExtensionInstalledBubble bubble(extension.get(), browser(), kEmptyBitmap);
    bubble.Initialize();
    EXPECT_NE(0, bubble.options() & ExtensionInstalledBubble::HOW_TO_USE);
  }
  {
    scoped_refptr<const extensions::Extension> extension =
        extensions::extension_action_test_util::CreateActionExtension(
            "Page action", extensions::extension_action_test_util::PAGE_ACTION);
    extension_service()->AddExtension(extension.get());
    ExtensionInstalledBubble bubble(extension.get(), browser(), kEmptyBitmap);
    bubble.Initialize();
    EXPECT_NE(0, bubble.options() & ExtensionInstalledBubble::HOW_TO_USE);
  }
}
