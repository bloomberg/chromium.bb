// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using PermissionsParserTest = ChromeManifestTest;

TEST_F(PermissionsParserTest, RemoveOverlappingAPIPermissions) {
  scoped_refptr<Extension> extension(LoadAndExpectWarning(
      "permissions_parser_overlapping_permissions.json",
      ErrorUtils::FormatErrorMessage(
          manifest_errors::kPermissionMarkedOptionalAndRequired, "tabs")));

  std::set<std::string> required_api_names =
      PermissionsParser::GetRequiredPermissions(extension.get())
          .GetAPIsAsStrings();

  std::set<std::string> optional_api_names =
      PermissionsParser::GetOptionalPermissions(extension.get())
          .GetAPIsAsStrings();

  // Check that required api permissions have not changed while "tabs" is
  // removed from optional permissions as it is specified as required.
  EXPECT_THAT(required_api_names,
              testing::UnorderedElementsAre("tabs", "storage"));
  EXPECT_THAT(optional_api_names, testing::UnorderedElementsAre("geolocation"));
}

}  // namespace extensions
