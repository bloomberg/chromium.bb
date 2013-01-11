// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "extensions/common/error_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::ErrorUtils;
using extensions::Extension;

namespace errors = extension_manifest_errors;

TEST_F(ExtensionManifestTest, WebAccessibleResources) {
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

  EXPECT_TRUE(extension1->HasWebAccessibleResources());
  EXPECT_FALSE(extension2->HasWebAccessibleResources());
  EXPECT_TRUE(extension3->HasWebAccessibleResources());
  EXPECT_FALSE(extension4->HasWebAccessibleResources());
  EXPECT_TRUE(extension5->HasWebAccessibleResources());
  EXPECT_TRUE(extension6->HasWebAccessibleResources());

  EXPECT_TRUE(extension1->IsResourceWebAccessible("test"));
  EXPECT_FALSE(extension1->IsResourceWebAccessible("none"));

  EXPECT_FALSE(extension2->IsResourceWebAccessible("test"));

  EXPECT_TRUE(extension3->IsResourceWebAccessible("test"));
  EXPECT_FALSE(extension3->IsResourceWebAccessible("none"));

  EXPECT_TRUE(extension4->IsResourceWebAccessible("test"));
  EXPECT_TRUE(extension4->IsResourceWebAccessible("none"));

  EXPECT_TRUE(extension5->IsResourceWebAccessible("anything"));
  EXPECT_TRUE(extension5->IsResourceWebAccessible("path/anything"));

  EXPECT_TRUE(extension6->IsResourceWebAccessible("path/anything.ext"));
  EXPECT_FALSE(extension6->IsResourceWebAccessible("anything.ext"));
  EXPECT_FALSE(extension6->IsResourceWebAccessible("path/anything.badext"));
}

TEST_F(ExtensionManifestTest, AppWebUrls) {
  Testcase testcases[] = {
    Testcase("web_urls_wrong_type.json", errors::kInvalidWebURLs),
    Testcase("web_urls_invalid_1.json",
             ErrorUtils::FormatErrorMessage(
                 errors::kInvalidWebURL,
                 base::IntToString(0),
                 errors::kExpectString)),
    Testcase("web_urls_invalid_2.json",
             ErrorUtils::FormatErrorMessage(
                 errors::kInvalidWebURL,
                 base::IntToString(0),
                 URLPattern::GetParseResultString(
                 URLPattern::PARSE_ERROR_MISSING_SCHEME_SEPARATOR))),
    Testcase("web_urls_invalid_3.json",
             ErrorUtils::FormatErrorMessage(
                 errors::kInvalidWebURL,
                 base::IntToString(0),
                 errors::kNoWildCardsInPaths)),
    Testcase("web_urls_invalid_4.json",
             ErrorUtils::FormatErrorMessage(
                 errors::kInvalidWebURL,
                 base::IntToString(0),
                 errors::kCannotClaimAllURLsInExtent)),
    Testcase("web_urls_invalid_5.json",
             ErrorUtils::FormatErrorMessage(
                 errors::kInvalidWebURL,
                 base::IntToString(1),
                 errors::kCannotClaimAllHostsInExtent))
  };
  RunTestcases(testcases, arraysize(testcases),
               EXPECT_TYPE_ERROR);

  LoadAndExpectSuccess("web_urls_has_port.json");

  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("web_urls_default.json"));
  ASSERT_EQ(1u, extension->web_extent().patterns().size());
  EXPECT_EQ("*://www.google.com/*",
            extension->web_extent().patterns().begin()->GetAsString());
}
