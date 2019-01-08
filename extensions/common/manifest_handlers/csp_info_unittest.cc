// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/csp_info.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "components/version_info/channel.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_test.h"

namespace extensions {

namespace {

namespace errors = manifest_errors;
namespace keys = manifest_keys;

std::string GetInvalidManifestKeyError(base::StringPiece key) {
  return ErrorUtils::FormatErrorMessage(errors::kInvalidManifestKey, key);
};

}  // namespace

using CSPInfoUnitTest = ManifestTest;

TEST_F(CSPInfoUnitTest, SandboxedPages) {
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

  // Sandboxed pages specified for a platform app with a custom CSP.
  scoped_refptr<Extension> extension6(
      LoadAndExpectSuccess("sandboxed_pages_valid_6.json"));

  // Sandboxed pages specified for a platform app with no custom CSP.
  scoped_refptr<Extension> extension7(
      LoadAndExpectSuccess("sandboxed_pages_valid_7.json"));

  const char kSandboxedCSP[] =
      "sandbox allow-scripts allow-forms allow-popups allow-modals; "
      "script-src 'self' 'unsafe-inline' 'unsafe-eval'; child-src 'self';";
  const char kDefaultCSP[] =
      "script-src 'self' blob: filesystem: chrome-extension-resource:; "
      "object-src 'self' blob: filesystem:;";
  const char kCustomSandboxedCSP[] =
      "sandbox; script-src 'self'; child-src 'self';";

  EXPECT_EQ(kSandboxedCSP, CSPInfo::GetResourceContentSecurityPolicy(
                               extension1.get(), "/test"));
  EXPECT_EQ(kDefaultCSP, CSPInfo::GetResourceContentSecurityPolicy(
                             extension1.get(), "/none"));
  EXPECT_EQ(kDefaultCSP, CSPInfo::GetResourceContentSecurityPolicy(
                             extension2.get(), "/test"));
  EXPECT_EQ(kCustomSandboxedCSP, CSPInfo::GetResourceContentSecurityPolicy(
                                     extension3.get(), "/test"));
  EXPECT_EQ(kDefaultCSP, CSPInfo::GetResourceContentSecurityPolicy(
                             extension3.get(), "/none"));
  EXPECT_EQ(kSandboxedCSP, CSPInfo::GetResourceContentSecurityPolicy(
                               extension4.get(), "/test"));
  EXPECT_EQ(kSandboxedCSP, CSPInfo::GetResourceContentSecurityPolicy(
                               extension5.get(), "/path/test.ext"));
  EXPECT_EQ(kDefaultCSP, CSPInfo::GetResourceContentSecurityPolicy(
                             extension5.get(), "/test"));
  EXPECT_EQ(kCustomSandboxedCSP, CSPInfo::GetResourceContentSecurityPolicy(
                                     extension6.get(), "/test"));
  EXPECT_EQ(kSandboxedCSP, CSPInfo::GetResourceContentSecurityPolicy(
                               extension7.get(), "/test"));

  Testcase testcases[] = {
      Testcase("sandboxed_pages_invalid_1.json",
               errors::kInvalidSandboxedPagesList),
      Testcase("sandboxed_pages_invalid_2.json", errors::kInvalidSandboxedPage),
      Testcase("sandboxed_pages_invalid_3.json",
               GetInvalidManifestKeyError(keys::kSandboxedPagesCSP)),
      Testcase("sandboxed_pages_invalid_4.json",
               GetInvalidManifestKeyError(keys::kSandboxedPagesCSP)),
      Testcase("sandboxed_pages_invalid_5.json",
               GetInvalidManifestKeyError(keys::kSandboxedPagesCSP))};
  RunTestcases(testcases, base::size(testcases), EXPECT_TYPE_ERROR);
}

TEST_F(CSPInfoUnitTest, CSPStringKey) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("csp_string_valid.json");
  ASSERT_TRUE(extension);
  EXPECT_EQ("script-src 'self'; default-src 'none';",
            CSPInfo::GetContentSecurityPolicy(extension.get()));

  RunTestcase(Testcase("csp_invalid_1.json", GetInvalidManifestKeyError(
                                                 keys::kContentSecurityPolicy)),
              EXPECT_TYPE_ERROR);
}

TEST_F(CSPInfoUnitTest, CSPDictionary_ExtensionPages) {
  const char kDefaultCSP[] =
      "script-src 'self' blob: filesystem: chrome-extension-resource:; "
      "object-src 'self' blob: filesystem:;";
  struct {
    const char* file_name;
    const char* csp;
  } cases[] = {
      {"csp_dictionary_valid.json", "default-src 'none';"},
      {"csp_empty_valid.json", "script-src 'self'; object-src 'self';"},
      {"csp_empty_dictionary_valid.json", kDefaultCSP}};

  // Verify that keys::kContentSecurityPolicy key can be used as a dictionary on
  // trunk.
  {
    ScopedCurrentChannel channel(version_info::Channel::UNKNOWN);
    for (const auto& test_case : cases) {
      SCOPED_TRACE(
          base::StringPrintf("%s on channel %s", test_case.file_name, "trunk"));
      scoped_refptr<Extension> extension =
          LoadAndExpectSuccess(test_case.file_name);
      ASSERT_TRUE(extension.get());
      EXPECT_EQ(test_case.csp,
                CSPInfo::GetContentSecurityPolicy(extension.get()));
    }
  }

  // Verify that keys::kContentSecurityPolicy key can't be used as a dictionary
  // on Stable.
  {
    ScopedCurrentChannel channel(version_info::Channel::STABLE);
    for (const auto& test_case : cases) {
      SCOPED_TRACE(base::StringPrintf("%s on channel %s", test_case.file_name,
                                      "stable"));
      LoadAndExpectError(
          test_case.file_name,
          GetInvalidManifestKeyError(keys::kContentSecurityPolicy));
    }
  }

  {
    ScopedCurrentChannel channel(version_info::Channel::UNKNOWN);
    Testcase testcases[] = {
        Testcase("csp_invalid_2.json",
                 GetInvalidManifestKeyError(
                     keys::kContentSecurityPolicy_ExtensionPagesPath)),
        Testcase("csp_invalid_3.json",
                 GetInvalidManifestKeyError(
                     keys::kContentSecurityPolicy_ExtensionPagesPath))};
    RunTestcases(testcases, base::size(testcases), EXPECT_TYPE_ERROR);
  }
}

TEST_F(CSPInfoUnitTest, CSPDictionary_Sandbox) {
  ScopedCurrentChannel channel(version_info::Channel::UNKNOWN);

  const char kCustomSandboxedCSP[] =
      "sandbox; script-src 'self'; child-src 'self';";
  const char kDefaultSandboxedPageCSP[] =
      "sandbox allow-scripts allow-forms allow-popups allow-modals; "
      "script-src 'self' 'unsafe-inline' 'unsafe-eval'; child-src 'self';";
  const char kDefaultExtensionPagesCSP[] =
      "script-src 'self' blob: filesystem: chrome-extension-resource:; "
      "object-src 'self' blob: filesystem:;";
  const char kCustomExtensionPagesCSP[] = "script-src; object-src;";

  struct {
    const char* file_name;
    const char* resource_path;
    const char* expected_csp;
  } success_cases[] = {
      {"sandbox_dictionary_1.json", "/test", kCustomSandboxedCSP},
      {"sandbox_dictionary_1.json", "/index", kDefaultExtensionPagesCSP},
      {"sandbox_dictionary_2.json", "/test", kDefaultSandboxedPageCSP},
      {"sandbox_dictionary_2.json", "/index", kCustomExtensionPagesCSP},
  };

  for (const auto& test_case : success_cases) {
    SCOPED_TRACE(base::StringPrintf("%s with path %s", test_case.file_name,
                                    test_case.resource_path));
    scoped_refptr<Extension> extension =
        LoadAndExpectSuccess(test_case.file_name);
    ASSERT_TRUE(extension);
    EXPECT_EQ(test_case.expected_csp,
              CSPInfo::GetResourceContentSecurityPolicy(
                  extension.get(), test_case.resource_path));
  }

  Testcase testcases[] = {
      {"sandbox_both_keys.json", errors::kSandboxPagesCSPKeyNotAllowed},
      {"sandbox_csp_with_dictionary.json",
       errors::kSandboxPagesCSPKeyNotAllowed},
      {"sandbox_invalid_type.json",
       GetInvalidManifestKeyError(
           keys::kContentSecurityPolicy_SandboxedPagesPath)},
      {"unsandboxed_csp.json",
       GetInvalidManifestKeyError(
           keys::kContentSecurityPolicy_SandboxedPagesPath)}};
  RunTestcases(testcases, base::size(testcases), EXPECT_TYPE_ERROR);
}

}  // namespace extensions
