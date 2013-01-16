// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"
#include "chrome/common/extensions/web_accessible_resources_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;
using extensions::WebAccessibleResourcesInfo;

class WebAccesibleResourcesManifestTest : public ExtensionManifestTest {
  virtual void SetUp() OVERRIDE {
    ExtensionManifestTest::SetUp();
    extensions::ManifestHandler::Register(
        extension_manifest_keys::kWebAccessibleResources,
        new extensions::WebAccessibleResourcesHandler);
  }
};

TEST_F(WebAccesibleResourcesManifestTest, WebAccessibleResources) {
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
      WebAccessibleResourcesInfo::HasWebAccessibleResources(extension1));
  EXPECT_FALSE(
      WebAccessibleResourcesInfo::HasWebAccessibleResources(extension2));
  EXPECT_TRUE(
      WebAccessibleResourcesInfo::HasWebAccessibleResources(extension3));
  EXPECT_FALSE(
      WebAccessibleResourcesInfo::HasWebAccessibleResources(extension4));
  EXPECT_TRUE(
      WebAccessibleResourcesInfo::HasWebAccessibleResources(extension5));
  EXPECT_TRUE(
      WebAccessibleResourcesInfo::HasWebAccessibleResources(extension6));

  EXPECT_TRUE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension1, "test"));
  EXPECT_FALSE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension1, "none"));

  EXPECT_FALSE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension2, "test"));

  EXPECT_TRUE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension3, "test"));
  EXPECT_FALSE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension3, "none"));

  EXPECT_TRUE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension4, "test"));
  EXPECT_TRUE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension4, "none"));

  EXPECT_TRUE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension5, "anything"));
  EXPECT_TRUE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension5, "path/anything"));

  EXPECT_TRUE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension6, "path/anything.ext"));
  EXPECT_FALSE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension6, "anything.ext"));
  EXPECT_FALSE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension6, "path/anything.badext"));
}
