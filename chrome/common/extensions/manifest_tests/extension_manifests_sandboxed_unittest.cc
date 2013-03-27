// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/csp_handler.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "chrome/common/extensions/manifest_handlers/sandboxed_page_info.h"
#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extension_manifest_errors;

namespace extensions {

class SandboxedPagesManifestTest : public ExtensionManifestTest {
 protected:
  virtual void SetUp() OVERRIDE {
    ExtensionManifestTest::SetUp();
    (new SandboxedPageHandler)->Register();
    (new CSPHandler(false))->Register();  // Not platform app.
  }
};

TEST_F(SandboxedPagesManifestTest, SandboxedPages) {
  // Sandboxed pages specified, no custom CSP value.
  scoped_refptr<Extension> extension1(
      LoadAndExpectSuccess("sandboxed_pages_valid_1.json"));

  // No sandboxed pages.
  scoped_refptr<Extension> extension2(
      LoadAndExpectSuccess("sandboxed_pages_valid_2.json"));

  // Sandboxed pages specified with a custom CSP value.
  scoped_refptr<Extension> extension3(
      LoadAndExpectSuccess("sandboxed_pages_valid_3.json"));

  // Sandboxed pages specified with wildcard, no custom CSP value.
  scoped_refptr<Extension> extension4(
      LoadAndExpectSuccess("sandboxed_pages_valid_4.json"));

  // Sandboxed pages specified with filename wildcard, no custom CSP value.
  scoped_refptr<Extension> extension5(
      LoadAndExpectSuccess("sandboxed_pages_valid_5.json"));

  const char kSandboxedCSP[] = "sandbox allow-scripts allow-forms allow-popups";
  const char kDefaultCSP[] =
      "script-src 'self' chrome-extension-resource:; object-src 'self'";
  const char kCustomSandboxedCSP[] =
      "sandbox; script-src: https://www.google.com";

  EXPECT_EQ(kSandboxedCSP,
            CSPInfo::GetResourceContentSecurityPolicy(extension1, "/test"));
  EXPECT_EQ(kDefaultCSP,
            CSPInfo::GetResourceContentSecurityPolicy(extension1, "/none"));
  EXPECT_EQ(kDefaultCSP,
            CSPInfo::GetResourceContentSecurityPolicy(extension2, "/test"));
  EXPECT_EQ(kCustomSandboxedCSP,
            CSPInfo::GetResourceContentSecurityPolicy(extension3, "/test"));
  EXPECT_EQ(kDefaultCSP,
            CSPInfo::GetResourceContentSecurityPolicy(extension3, "/none"));
  EXPECT_EQ(kSandboxedCSP,
            CSPInfo::GetResourceContentSecurityPolicy(extension4, "/test"));
  EXPECT_EQ(
      kSandboxedCSP,
      CSPInfo::GetResourceContentSecurityPolicy(extension5, "/path/test.ext"));
  EXPECT_EQ(kDefaultCSP,
            CSPInfo::GetResourceContentSecurityPolicy(extension5, "/test"));

  Testcase testcases[] = {
    Testcase("sandboxed_pages_invalid_1.json",
        errors::kInvalidSandboxedPagesList),
    Testcase("sandboxed_pages_invalid_2.json",
        errors::kInvalidSandboxedPage),
    Testcase("sandboxed_pages_invalid_3.json",
        errors::kInvalidSandboxedPagesCSP),
    Testcase("sandboxed_pages_invalid_4.json",
        errors::kInvalidSandboxedPagesCSP),
    Testcase("sandboxed_pages_invalid_5.json",
        errors::kInvalidSandboxedPagesCSP)
  };
  RunTestcases(testcases, arraysize(testcases),
               EXPECT_TYPE_ERROR);
}

}  // namespace extensions
