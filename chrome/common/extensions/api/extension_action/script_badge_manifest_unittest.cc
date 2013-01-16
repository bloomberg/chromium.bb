// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/extensions/api/extension_action/script_badge_handler.h"
#include "chrome/common/extensions/extension_builder.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extension_manifest_errors;

namespace extensions {

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

class ScriptBadgeManifestTest : public ExtensionManifestTest {
 protected:
  virtual void SetUp() OVERRIDE {
    ManifestHandler::Register(extension_manifest_keys::kScriptBadge,
                              new ScriptBadgeHandler);
  }
};

}  // namespace

TEST_F(ScriptBadgeManifestTest, ScriptBadgeBasic) {
  scoped_refptr<Extension> extension(
      ExtensionBuilder()
      .SetManifest(DictionaryBuilder()
                   .Set("manifest_version", 2)
                   .Set("name", "my extension")
                   .Set("version", "1.0.0.0")
                   .Set("description",
                        "Check that a simple script_badge section parses")
                   .Set("icons", DictionaryBuilder()
                        .Set("16", "icon16.png")
                        .Set("32", "icon32.png")
                        .Set("19", "icon19.png")
                        .Set("48", "icon48.png"))
                   .Set("script_badge", DictionaryBuilder()
                        .Set("default_popup", "popup.html")))
      .Build());
  ASSERT_TRUE(extension.get());
  const ActionInfo* script_badge_info =
      ActionInfo::GetScriptBadgeInfo(extension);
  ASSERT_TRUE(script_badge_info);
  EXPECT_THAT(StripMissingFlagWarning(extension->install_warnings()),
              testing::ElementsAre(/*empty*/));

  const ExtensionIconSet& default_icon =
      script_badge_info->default_icon;
  // Should have a default icon set.
  ASSERT_FALSE(default_icon.empty());

  // Verify that correct icon paths are registered in default_icon.
  EXPECT_EQ(2u, default_icon.map().size());
  EXPECT_EQ("icon16.png",
            default_icon.Get(extension_misc::EXTENSION_ICON_BITTY,
                             ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("icon32.png",
            default_icon.Get(2 * extension_misc::EXTENSION_ICON_BITTY,
                             ExtensionIconSet::MATCH_EXACTLY));

  EXPECT_EQ("my extension", script_badge_info->default_title);
  EXPECT_FALSE(script_badge_info->default_popup_url.is_empty());
}

TEST_F(ScriptBadgeManifestTest, ScriptBadgeExplicitTitleAndIconsIgnored) {
  scoped_refptr<Extension> extension(
      ExtensionBuilder()
      .SetManifest(DictionaryBuilder()
                   .Set("manifest_version", 2)
                   .Set("name", "my extension")
                   .Set("version", "1.0.0.0")
                   .Set("description",
                        "Check that a simple script_badge section parses")
                   .Set("icons", DictionaryBuilder()
                        .Set("16", "icon16.png"))
                   .Set("script_badge", DictionaryBuilder()
                        .Set("default_title", "Other Extension")
                        .Set("default_icon", "malicious.png")))
      .Build());
  ASSERT_TRUE(extension.get());
  const ActionInfo* script_badge_info =
      ActionInfo::GetScriptBadgeInfo(extension);
  ASSERT_TRUE(script_badge_info);

  EXPECT_THAT(StripMissingFlagWarning(extension->install_warnings()),
              testing::ElementsAre(
                  Extension::InstallWarning(
                      Extension::InstallWarning::FORMAT_TEXT,
                      errors::kScriptBadgeTitleIgnored),
                  Extension::InstallWarning(
                      Extension::InstallWarning::FORMAT_TEXT,
                      errors::kScriptBadgeIconIgnored)));

  const ExtensionIconSet& default_icon =
      script_badge_info->default_icon;
  ASSERT_FALSE(default_icon.empty());

  EXPECT_EQ(1u, default_icon.map().size());
  EXPECT_EQ("icon16.png",
            default_icon.Get(extension_misc::EXTENSION_ICON_BITTY,
                             ExtensionIconSet::MATCH_EXACTLY));

  EXPECT_EQ("my extension", script_badge_info->default_title);
}

TEST_F(ScriptBadgeManifestTest, ScriptBadgeIconFallsBackToPuzzlePiece) {
  scoped_refptr<Extension> extension(
      ExtensionBuilder()
      .SetManifest(DictionaryBuilder()
                   .Set("manifest_version", 2)
                   .Set("name", "my extension")
                   .Set("version", "1.0.0.0")
                   .Set("description",
                        "Check that a simple script_badge section parses")
                   .Set("icons", DictionaryBuilder()
                        .Set("128", "icon128.png")))
      .Build());
  ASSERT_TRUE(extension.get());
  const ActionInfo* script_badge_info =
      ActionInfo::GetScriptBadgeInfo(extension);
  ASSERT_TRUE(script_badge_info);
  EXPECT_THAT(extension->install_warnings(),
              testing::ElementsAre(/*empty*/));

  const ExtensionIconSet& default_icon =
      script_badge_info->default_icon;
  ASSERT_FALSE(default_icon.empty()) << "Should fall back to the 128px icon.";
  EXPECT_EQ(2u, default_icon.map().size());
  EXPECT_EQ("icon128.png",
            default_icon.Get(extension_misc::EXTENSION_ICON_BITTY,
                             ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("icon128.png",
            default_icon.Get(2 * extension_misc::EXTENSION_ICON_BITTY,
                             ExtensionIconSet::MATCH_EXACTLY));
}

}  // namespace extensions
