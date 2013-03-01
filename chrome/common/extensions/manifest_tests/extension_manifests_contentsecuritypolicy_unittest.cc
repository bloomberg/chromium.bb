// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/csp_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extension_manifest_errors;

class ContentSecurityPolicyManifestTest : public ExtensionManifestTest {
  virtual void SetUp() OVERRIDE {
    ExtensionManifestTest::SetUp();
    (new extensions::CSPHandler(false))->Register(); //  not platform app.
  }
};

TEST_F(ContentSecurityPolicyManifestTest, InsecureContentSecurityPolicy) {
  Testcase testcases[] = {
    Testcase("insecure_contentsecuritypolicy_1.json",
        errors::kInsecureContentSecurityPolicy),
    Testcase("insecure_contentsecuritypolicy_2.json",
        errors::kInsecureContentSecurityPolicy),
    Testcase("insecure_contentsecuritypolicy_3.json",
        errors::kInsecureContentSecurityPolicy),
  };
  RunTestcases(testcases, arraysize(testcases), EXPECT_TYPE_ERROR);
}
