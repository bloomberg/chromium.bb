// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/requirements_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace errors = manifest_errors;

class RequirementsManifestTest : public ChromeManifestTest {
};

TEST_F(RequirementsManifestTest, RequirementsInvalid) {
  Testcase testcases[] = {
    Testcase("requirements_invalid_requirements.json",
             errors::kInvalidRequirements),
    Testcase("requirements_invalid_keys.json", errors::kInvalidRequirements),
    Testcase("requirements_invalid_3d.json",
             ErrorUtils::FormatErrorMessage(
                 errors::kInvalidRequirement, "3D")),
    Testcase("requirements_invalid_3d_features.json",
             ErrorUtils::FormatErrorMessage(
                 errors::kInvalidRequirement, "3D")),
    Testcase("requirements_invalid_3d_features_value.json",
             ErrorUtils::FormatErrorMessage(
                 errors::kInvalidRequirement, "3D")),
    Testcase("requirements_invalid_3d_no_features.json",
             ErrorUtils::FormatErrorMessage(
                 errors::kInvalidRequirement, "3D")),
    Testcase("requirements_invalid_plugins.json",
             ErrorUtils::FormatErrorMessage(
                 errors::kInvalidRequirement, "plugins")),
    Testcase("requirements_invalid_plugins_key.json",
             ErrorUtils::FormatErrorMessage(
                 errors::kInvalidRequirement, "plugins")),
    Testcase("requirements_invalid_plugins_value.json",
             ErrorUtils::FormatErrorMessage(
                 errors::kInvalidRequirement, "plugins"))
  };

  RunTestcases(testcases, arraysize(testcases), EXPECT_TYPE_ERROR);
}

TEST_F(RequirementsManifestTest, RequirementsValid) {
  // Test the defaults.
  scoped_refptr<Extension> extension(LoadAndExpectSuccess(
      "requirements_valid_empty.json"));
  ASSERT_TRUE(extension.get());
  EXPECT_EQ(RequirementsInfo::GetRequirements(extension.get()).webgl, false);
  EXPECT_EQ(RequirementsInfo::GetRequirements(extension.get()).npapi, false);

  // Test loading all the requirements.
  extension = LoadAndExpectSuccess("requirements_valid_full.json");
  ASSERT_TRUE(extension.get());
  EXPECT_EQ(RequirementsInfo::GetRequirements(extension.get()).webgl, true);
  EXPECT_EQ(RequirementsInfo::GetRequirements(extension.get()).npapi, true);
}

// When an npapi plugin is present, the default of the "npapi" requirement
// changes.
TEST_F(RequirementsManifestTest, RequirementsNpapiDefault) {
  scoped_refptr<Extension> extension(LoadAndExpectSuccess(
      "requirements_npapi_empty.json"));
  ASSERT_TRUE(extension.get());
  EXPECT_EQ(RequirementsInfo::GetRequirements(extension.get()).webgl, false);
  EXPECT_EQ(RequirementsInfo::GetRequirements(extension.get()).npapi, true);

  extension = LoadAndExpectSuccess(
      "requirements_npapi_empty_plugins_empty.json");
  ASSERT_TRUE(extension.get());
  EXPECT_EQ(RequirementsInfo::GetRequirements(extension.get()).webgl, false);
  EXPECT_EQ(RequirementsInfo::GetRequirements(extension.get()).npapi, false);

  extension = LoadAndExpectSuccess("requirements_npapi.json");
  ASSERT_TRUE(extension.get());
  EXPECT_EQ(RequirementsInfo::GetRequirements(extension.get()).webgl, false);
  EXPECT_EQ(RequirementsInfo::GetRequirements(extension.get()).npapi, false);

  extension = LoadAndExpectSuccess("requirements_npapi_plugins_empty.json");
  ASSERT_TRUE(extension.get());
  EXPECT_EQ(RequirementsInfo::GetRequirements(extension.get()).webgl, false);
  EXPECT_EQ(RequirementsInfo::GetRequirements(extension.get()).npapi, true);
}

}  // namespace extensions
