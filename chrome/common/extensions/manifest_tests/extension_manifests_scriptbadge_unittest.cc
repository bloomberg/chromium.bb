// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/extension_switch_utils.h"
#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extension_manifest_errors;
namespace switch_utils = extensions::switch_utils;
using extensions::Extension;

namespace {

std::vector<Extension::InstallWarning> StripMissingFlagWarning(
    const std::vector<Extension::InstallWarning>& install_warnings) {
  std::vector<Extension::InstallWarning> result;
  for (size_t i = 0; i < install_warnings.size(); ++i) {
    if (install_warnings[i].message != errors::kScriptBadgeRequiresFlag)
      result.push_back(install_warnings[i]);
  }
  return result;
}

TEST_F(ExtensionManifestTest, ScriptBadgeBasic) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("script_badge_basic.json"));
  ASSERT_TRUE(extension.get());
  ASSERT_TRUE(extension->script_badge());
  EXPECT_THAT(StripMissingFlagWarning(extension->install_warnings()),
              testing::ElementsAre(/*empty*/));

  const ExtensionIconSet* default_icon =
      extension->script_badge()->default_icon();
  // Default icon set should not be NULL.
  ASSERT_TRUE(default_icon);

  // Verify that correct icon paths are registered in default_icon.
  EXPECT_EQ(2u, default_icon->map().size());
  EXPECT_EQ("icon16.png",
            default_icon->Get(extension_misc::EXTENSION_ICON_BITTY,
                              ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("icon32.png",
            default_icon->Get(2 * extension_misc::EXTENSION_ICON_BITTY,
                              ExtensionIconSet::MATCH_EXACTLY));

  EXPECT_EQ("my extension", extension->script_badge()->GetTitle(
      ExtensionAction::kDefaultTabId));
  EXPECT_TRUE(extension->script_badge()->HasPopup(
      ExtensionAction::kDefaultTabId));
}

TEST_F(ExtensionManifestTest, ScriptBadgeExplicitTitleAndIconsIgnored) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("script_badge_title_icons_ignored.json"));
  ASSERT_TRUE(extension.get());
  ASSERT_TRUE(extension->script_badge());

  EXPECT_THAT(StripMissingFlagWarning(extension->install_warnings()),
              testing::ElementsAre(
                  Extension::InstallWarning(
                      Extension::InstallWarning::FORMAT_TEXT,
                      errors::kScriptBadgeTitleIgnored),
                  Extension::InstallWarning(
                      Extension::InstallWarning::FORMAT_TEXT,
                      errors::kScriptBadgeIconIgnored)));

  const ExtensionIconSet* default_icon =
      extension->script_badge()->default_icon();
  ASSERT_TRUE(default_icon);

  EXPECT_EQ(1u, default_icon->map().size());
  EXPECT_EQ("icon16.png",
            default_icon->Get(extension_misc::EXTENSION_ICON_BITTY,
                              ExtensionIconSet::MATCH_EXACTLY));

  EXPECT_EQ("my extension", extension->script_badge()->GetTitle(
      ExtensionAction::kDefaultTabId));
}

TEST_F(ExtensionManifestTest, ScriptBadgeIconFallsBackToPuzzlePiece) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("script_badge_only_use_icon16.json"));
  ASSERT_TRUE(extension.get());
  ASSERT_TRUE(extension->script_badge());
  EXPECT_THAT(extension->install_warnings(),
              testing::ElementsAre(/*empty*/));

  EXPECT_FALSE(extension->script_badge()->default_icon())
      << "Should not fall back to the 64px icon.";
  EXPECT_EQ(NULL, extension->script_badge()->default_icon());
}

}  // namespace
