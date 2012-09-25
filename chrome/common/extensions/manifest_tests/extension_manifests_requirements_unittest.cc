// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extension_manifest_errors;

TEST_F(ExtensionManifestTest, RequirementsInvalid) {
  Testcase testcases[] = {
    Testcase("requirements_invalid_requirements.json",
             errors::kInvalidRequirements),
    Testcase("requirements_invalid_keys.json", errors::kInvalidRequirements),
    Testcase("requirements_invalid_3d.json",
             ExtensionErrorUtils::FormatErrorMessage(
                 errors::kInvalidRequirement, "3D")),
    Testcase("requirements_invalid_3d_features.json",
             ExtensionErrorUtils::FormatErrorMessage(
                 errors::kInvalidRequirement, "3D")),
    Testcase("requirements_invalid_3d_features_value.json",
             ExtensionErrorUtils::FormatErrorMessage(
                 errors::kInvalidRequirement, "3D")),
    Testcase("requirements_invalid_3d_no_features.json",
             ExtensionErrorUtils::FormatErrorMessage(
                 errors::kInvalidRequirement, "3D")),
    Testcase("requirements_invalid_plugins.json",
             ExtensionErrorUtils::FormatErrorMessage(
                 errors::kInvalidRequirement, "plugins")),
    Testcase("requirements_invalid_plugins_key.json",
             ExtensionErrorUtils::FormatErrorMessage(
                 errors::kInvalidRequirement, "plugins")),
    Testcase("requirements_invalid_plugins_value.json",
             ExtensionErrorUtils::FormatErrorMessage(
                 errors::kInvalidRequirement, "plugins"))
  };

  RunTestcases(testcases, arraysize(testcases), EXPECT_TYPE_ERROR);
}

TEST_F(ExtensionManifestTest, RequirementsValid) {
  // Test the defaults.
  scoped_refptr<extensions::Extension> extension(LoadAndExpectSuccess(
      "requirements_valid_empty.json"));
  ASSERT_TRUE(extension.get());
  EXPECT_EQ(extension->requirements().webgl, false);
  EXPECT_EQ(extension->requirements().css3d, false);
  EXPECT_EQ(extension->requirements().npapi, false);

  // Test loading all the requirements.
  extension = LoadAndExpectSuccess("requirements_valid_full.json");
  ASSERT_TRUE(extension.get());
  EXPECT_EQ(extension->requirements().webgl, true);
  EXPECT_EQ(extension->requirements().css3d, true);
  EXPECT_EQ(extension->requirements().npapi, true);
}

// When an npapi plugin is present, the default of the "npapi" requirement
// changes.
TEST_F(ExtensionManifestTest, RequirementsNpapiDefault) {
  scoped_refptr<extensions::Extension> extension(LoadAndExpectSuccess(
      "requirements_npapi_empty.json"));
  ASSERT_TRUE(extension.get());
  EXPECT_EQ(extension->requirements().webgl, false);
  EXPECT_EQ(extension->requirements().css3d, false);
#if defined(OS_CHROMEOS)
  EXPECT_EQ(extension->requirements().npapi, false);
#else
  EXPECT_EQ(extension->requirements().npapi, true);
#endif  // defined(OS_CHROMEOS)

  extension = LoadAndExpectSuccess("requirements_npapi.json");
  ASSERT_TRUE(extension.get());
  EXPECT_EQ(extension->requirements().webgl, false);
  EXPECT_EQ(extension->requirements().css3d, false);
  EXPECT_EQ(extension->requirements().npapi, false);
}
