// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

#include "chrome/common/extensions/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST_F(ExtensionManifestTest, NormalizeIconPaths) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("normalize_icon_paths.json"));
  EXPECT_EQ("16.png", extension->icons().Get(
      ExtensionIconSet::EXTENSION_ICON_BITTY, ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("48.png", extension->icons().Get(
      ExtensionIconSet::EXTENSION_ICON_MEDIUM,
      ExtensionIconSet::MATCH_EXACTLY));
}

TEST_F(ExtensionManifestTest, InvalidIconSizes) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("init_ignored_icon_size.json"));
  EXPECT_EQ("", extension->icons().Get(
      static_cast<ExtensionIconSet::Icons>(300),
      ExtensionIconSet::MATCH_EXACTLY));
}

TEST_F(ExtensionManifestTest, ValidIconSizes) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("init_valid_icon_size.json"));
  EXPECT_EQ("16.png", extension->icons().Get(
      ExtensionIconSet::EXTENSION_ICON_BITTY, ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("24.png", extension->icons().Get(
      ExtensionIconSet::EXTENSION_ICON_SMALLISH,
      ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("32.png", extension->icons().Get(
      ExtensionIconSet::EXTENSION_ICON_SMALL, ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("48.png", extension->icons().Get(
      ExtensionIconSet::EXTENSION_ICON_MEDIUM,
      ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("128.png", extension->icons().Get(
      ExtensionIconSet::EXTENSION_ICON_LARGE, ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("256.png", extension->icons().Get(
      ExtensionIconSet::EXTENSION_ICON_EXTRA_LARGE,
      ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("512.png", extension->icons().Get(
      ExtensionIconSet::EXTENSION_ICON_GIGANTOR,
      ExtensionIconSet::MATCH_EXACTLY));
}
