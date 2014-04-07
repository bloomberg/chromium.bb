// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace errors = manifest_errors;

namespace {

class BrowserActionManifestTest : public ExtensionManifestTest {
};

TEST_F(BrowserActionManifestTest,
       BrowserActionManifestIcons_NoDefaultIcons) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
      .SetManifest(DictionaryBuilder()
                   .Set("name", "No default properties")
                   .Set("version", "1.0.0")
                   .Set("manifest_version", 2)
                   .Set("browser_action", DictionaryBuilder()
                       .Set("default_title", "Title")))
      .Build();

  ASSERT_TRUE(extension.get());
  const ActionInfo* browser_action_info =
      ActionInfo::GetBrowserActionInfo(extension.get());
  ASSERT_TRUE(browser_action_info);
  EXPECT_TRUE(browser_action_info->default_icon.empty());
}

TEST_F(BrowserActionManifestTest,
       BrowserActionManifestIcons_StringDefaultIcon) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
      .SetManifest(DictionaryBuilder()
                   .Set("name", "String default icon")
                   .Set("version", "1.0.0")
                   .Set("manifest_version", 2)
                   .Set("browser_action", DictionaryBuilder()
                       .Set("default_icon", "icon.png")))
      .Build();

  ASSERT_TRUE(extension.get());
  const ActionInfo* browser_action_info =
      ActionInfo::GetBrowserActionInfo(extension.get());
  ASSERT_TRUE(browser_action_info);
  ASSERT_FALSE(browser_action_info->default_icon.empty());

  const ExtensionIconSet& icons = browser_action_info->default_icon;

  EXPECT_EQ(1u, icons.map().size());
  EXPECT_EQ("icon.png", icons.Get(19, ExtensionIconSet::MATCH_EXACTLY));
}

TEST_F(BrowserActionManifestTest,
       BrowserActionManifestIcons_DictDefaultIcon) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
      .SetManifest(DictionaryBuilder()
                   .Set("name", "Dictionary default icon")
                   .Set("version", "1.0.0")
                   .Set("manifest_version", 2)
                   .Set("browser_action", DictionaryBuilder()
                       .Set("default_icon", DictionaryBuilder()
                           .Set("19", "icon19.png")
                           .Set("24", "icon24.png")  // Should be ignored.
                           .Set("38", "icon38.png"))))
      .Build();

  ASSERT_TRUE(extension.get());
  const ActionInfo* browser_action_info =
      ActionInfo::GetBrowserActionInfo(extension.get());
  ASSERT_TRUE(browser_action_info);
  ASSERT_FALSE(browser_action_info->default_icon.empty());

  const ExtensionIconSet& icons = browser_action_info->default_icon;

  // 24px icon should be ignored.
  EXPECT_EQ(2u, icons.map().size());
  EXPECT_EQ("icon19.png", icons.Get(19, ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("icon38.png", icons.Get(38, ExtensionIconSet::MATCH_EXACTLY));
}

TEST_F(BrowserActionManifestTest,
       BrowserActionManifestIcons_InvalidDefaultIcon) {
  scoped_ptr<base::DictionaryValue> manifest_value = DictionaryBuilder()
      .Set("name", "Invalid default icon").Set("version", "1.0.0")
      .Set("manifest_version", 2)
      .Set("browser_action",
           DictionaryBuilder().Set(
               "default_icon",
               DictionaryBuilder().Set("19", std::string())  // Invalid value.
                   .Set("24", "icon24.png").Set("38", "icon38.png"))).Build();

  base::string16 error = ErrorUtils::FormatErrorMessageUTF16(
      errors::kInvalidIconPath, "19");
  LoadAndExpectError(Manifest(manifest_value.get(), "Invalid default icon"),
                     errors::kInvalidIconPath);
}

}  // namespace
}  // namespace extensions
