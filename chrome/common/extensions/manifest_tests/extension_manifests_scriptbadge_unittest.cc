// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/extension_switch_utils.h"
#include "grit/theme_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/skia_util.h"

namespace errors = extension_manifest_errors;
namespace switch_utils = extensions::switch_utils;
using extensions::Extension;

namespace {

bool ImagesAreEqual(const gfx::Image& i1, const gfx::Image& i2) {
  return gfx::BitmapsAreEqual(*i1.ToSkBitmap(), *i2.ToSkBitmap());
}

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

  EXPECT_EQ("my extension", extension->script_badge()->GetTitle(
      ExtensionAction::kDefaultTabId));
  EXPECT_TRUE(extension->script_badge()->HasPopup(
      ExtensionAction::kDefaultTabId));
  EXPECT_TRUE(ImagesAreEqual(
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_EXTENSIONS_FAVICON),
      extension->script_badge()->GetIcon(ExtensionAction::kDefaultTabId)));
  EXPECT_EQ("icon16.png", extension->script_badge()->default_icon_path());
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
  EXPECT_EQ("my extension", extension->script_badge()->GetTitle(
      ExtensionAction::kDefaultTabId));
  EXPECT_TRUE(ImagesAreEqual(
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_EXTENSIONS_FAVICON),
      extension->script_badge()->GetIcon(ExtensionAction::kDefaultTabId)));
  EXPECT_EQ("icon16.png", extension->script_badge()->default_icon_path());
}

TEST_F(ExtensionManifestTest, ScriptBadgeIconFallsBackToPuzzlePiece) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("script_badge_only_use_icon16.json"));
  ASSERT_TRUE(extension.get());
  ASSERT_TRUE(extension->script_badge());
  EXPECT_THAT(extension->install_warnings(),
              testing::ElementsAre(/*empty*/));

  EXPECT_EQ("", extension->script_badge()->default_icon_path())
      << "Should not fall back to the 64px icon.";
  EXPECT_FALSE(extension->script_badge()->GetIcon(
      ExtensionAction::kDefaultTabId).IsEmpty())
      << "Should set the puzzle piece as the default, but there's no way "
      << "to assert in a unittest what the image looks like.";
}

}  // namespace
