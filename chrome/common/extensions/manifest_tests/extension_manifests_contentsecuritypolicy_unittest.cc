// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stl_util.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extensions::manifest_errors;
namespace keys = extensions::manifest_keys;
using extensions::ErrorUtils;

class ContentSecurityPolicyManifestTest : public ChromeManifestTest {
};

TEST_F(ContentSecurityPolicyManifestTest, InsecureContentSecurityPolicy) {
  Testcase testcases[] = {
      Testcase("insecure_contentsecuritypolicy_1.json",
               ErrorUtils::FormatErrorMessage(
                   errors::kInvalidCSPInsecureValueIgnored,
                   keys::kContentSecurityPolicy, "http://example.com",
                   "script-src")),
      Testcase(
          "insecure_contentsecuritypolicy_2.json",
          ErrorUtils::FormatErrorMessage(
              errors::kInvalidCSPInsecureValueIgnored,
              keys::kContentSecurityPolicy, "'unsafe-inline'", "script-src")),
      Testcase("insecure_contentsecuritypolicy_3.json",
               ErrorUtils::FormatErrorMessage(
                   errors::kInvalidCSPMissingSecureSrc,
                   keys::kContentSecurityPolicy, "object-src"))};
  RunTestcases(testcases, base::size(testcases), EXPECT_TYPE_WARNING);
}
