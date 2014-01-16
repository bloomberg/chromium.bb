// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;
using extensions::WebAccessibleResourcesInfo;

class WebAccessibleResourcesManifestTest : public ExtensionManifestTest {
};

TEST_F(WebAccessibleResourcesManifestTest, WebAccessibleResources) {
  // Manifest version 2 with web accessible resources specified.
  scoped_refptr<Extension> extension1(
      LoadAndExpectSuccess("web_accessible_resources_1.json"));

  // Manifest version 2 with no web accessible resources.
  scoped_refptr<Extension> extension2(
      LoadAndExpectSuccess("web_accessible_resources_2.json"));

  // Default manifest version with web accessible resources specified.
  scoped_refptr<Extension> extension3(
      LoadAndExpectSuccess("web_accessible_resources_3.json"));

  // Default manifest version with no web accessible resources.
  scoped_refptr<Extension> extension4(
      LoadAndExpectSuccess("web_accessible_resources_4.json"));

  // Default manifest version with wildcard web accessible resource.
  scoped_refptr<Extension> extension5(
      LoadAndExpectSuccess("web_accessible_resources_5.json"));

  // Default manifest version with wildcard with specific path and extension.
  scoped_refptr<Extension> extension6(
      LoadAndExpectSuccess("web_accessible_resources_6.json"));

  EXPECT_TRUE(
      WebAccessibleResourcesInfo::HasWebAccessibleResources(extension1.get()));
  EXPECT_FALSE(
      WebAccessibleResourcesInfo::HasWebAccessibleResources(extension2.get()));
  EXPECT_TRUE(
      WebAccessibleResourcesInfo::HasWebAccessibleResources(extension3.get()));
  EXPECT_FALSE(
      WebAccessibleResourcesInfo::HasWebAccessibleResources(extension4.get()));
  EXPECT_TRUE(
      WebAccessibleResourcesInfo::HasWebAccessibleResources(extension5.get()));
  EXPECT_TRUE(
      WebAccessibleResourcesInfo::HasWebAccessibleResources(extension6.get()));

  EXPECT_TRUE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension1.get(), "test"));
  EXPECT_FALSE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension1.get(), "none"));

  EXPECT_FALSE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension2.get(), "test"));

  EXPECT_TRUE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension3.get(), "test"));
  EXPECT_FALSE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension3.get(), "none"));

  EXPECT_TRUE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension4.get(), "test"));
  EXPECT_TRUE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension4.get(), "none"));

  EXPECT_TRUE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension5.get(), "anything"));
  EXPECT_TRUE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension5.get(), "path/anything"));

  EXPECT_TRUE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension6.get(), "path/anything.ext"));
  EXPECT_FALSE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension6.get(), "anything.ext"));
  EXPECT_FALSE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension6.get(), "path/anything.badext"));
}
