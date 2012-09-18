// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_builder.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"
#include "chrome/common/extensions/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extension_manifest_errors;

namespace extensions {
namespace {

TEST_F(ExtensionManifestTest, BrowserActionManifestIcons_NoDefaultIcons) {
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
  ASSERT_TRUE(extension->browser_action());
  EXPECT_FALSE(extension->browser_action()->default_icon());
}


TEST_F(ExtensionManifestTest, BrowserActionManifestIcons_StringDefaultIcon) {
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
  ASSERT_TRUE(extension->browser_action());
  ASSERT_TRUE(extension->browser_action()->default_icon());

  const ExtensionIconSet* icons = extension->browser_action()->default_icon();

  EXPECT_EQ(1u, icons->map().size());
  EXPECT_EQ("icon.png", icons->Get(19, ExtensionIconSet::MATCH_EXACTLY));
}

TEST_F(ExtensionManifestTest, BrowserActionManifestIcons_DictDefaultIcon) {
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
  ASSERT_TRUE(extension->browser_action());
  ASSERT_TRUE(extension->browser_action()->default_icon());

  const ExtensionIconSet* icons = extension->browser_action()->default_icon();

  // 24px icon should be ignored.
  EXPECT_EQ(2u, icons->map().size());
  EXPECT_EQ("icon19.png", icons->Get(19, ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("icon38.png", icons->Get(38, ExtensionIconSet::MATCH_EXACTLY));
}

TEST_F(ExtensionManifestTest, BrowserActionManifestIcons_InvalidDefaultIcon) {
  scoped_ptr<DictionaryValue> manifest_value = DictionaryBuilder()
      .Set("name", "Invalid default icon")
      .Set("version", "1.0.0")
      .Set("manifest_version", 2)
      .Set("browser_action", DictionaryBuilder()
          .Set("default_icon", DictionaryBuilder()
              .Set("19", "")  // Invalid value.
              .Set("24", "icon24.png")
              .Set("38", "icon38.png")))
     .Build();

  string16 error = ExtensionErrorUtils::FormatErrorMessageUTF16(
      errors::kInvalidIconPath, "19");
  LoadAndExpectError(Manifest(manifest_value.get(), "Invalid default icon"),
                     errors::kInvalidIconPath);
}

}  // namespace
}  // namespace extensions
