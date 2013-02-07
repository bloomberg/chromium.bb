// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extension_manifest_errors;

TEST_F(ExtensionManifestTest, PlatformApps) {
  scoped_refptr<extensions::Extension> extension =
      LoadAndExpectSuccess("init_valid_platform_app.json");
  EXPECT_TRUE(extension->is_storage_isolated());
  EXPECT_TRUE(extension->incognito_split_mode());

  extension =
      LoadAndExpectSuccess("init_valid_platform_app_no_manifest_version.json");
  EXPECT_EQ(2, extension->manifest_version());

  extension = LoadAndExpectSuccess("incognito_valid_platform_app.json");
  EXPECT_TRUE(extension->incognito_split_mode());

  Testcase error_testcases[] = {
    Testcase("init_invalid_platform_app_2.json",
        errors::kBackgroundRequiredForPlatformApps),
    Testcase("init_invalid_platform_app_3.json",
        errors::kPlatformAppNeedsManifestVersion2),
    Testcase("incognito_invalid_platform_app.json",
        errors::kInvalidIncognitoModeForPlatformApp),
  };
  RunTestcases(error_testcases, arraysize(error_testcases), EXPECT_TYPE_ERROR);

  Testcase warning_testcases[] = {
    Testcase(
        "init_invalid_platform_app_1.json",
        "'app.launch' is only allowed for hosted apps and legacy packaged "
            "apps, and this is a packaged app."),
    Testcase(
        "init_invalid_platform_app_4.json",
        "'background' is only allowed for extensions, hosted apps and legacy "
            "packaged apps, and this is a packaged app."),
    Testcase(
        "init_invalid_platform_app_5.json",
        "'background' is only allowed for extensions, hosted apps and legacy "
            "packaged apps, and this is a packaged app.")
  };
  RunTestcases(
      warning_testcases, arraysize(warning_testcases), EXPECT_TYPE_WARNING);
}

TEST_F(ExtensionManifestTest, PlatformAppContentSecurityPolicy) {
  // Normal platform apps can't specify a CSP value.
  Testcase warning_testcases[] = {
    Testcase(
        "init_platform_app_csp_warning_1.json",
        "'content_security_policy' is only allowed for extensions and legacy "
            "packaged apps, and this is a packaged app."),
    Testcase(
        "init_platform_app_csp_warning_2.json",
        "'app.content_security_policy' is not allowed for specified extension "
            "ID.")
  };
  RunTestcases(
      warning_testcases, arraysize(warning_testcases), EXPECT_TYPE_WARNING);

  // Whitelisted ones can (this is the ID corresponding to the base 64 encoded
  // key in the init_platform_app_csp.json manifest.)
  std::string test_id = "ahplfneplbnjcflhdgkkjeiglkkfeelb";
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kWhitelistedExtensionID, test_id);
  scoped_refptr<extensions::Extension> extension =
      LoadAndExpectSuccess("init_platform_app_csp.json");
  EXPECT_EQ(0U, extension->install_warnings().size())
      << "Unexpected warning " << extension->install_warnings()[0].message;
  EXPECT_TRUE(extension->is_platform_app());
  EXPECT_EQ(
      "default-src 'self' https://www.google.com",
      extension->GetResourceContentSecurityPolicy(""));

  // But even whitelisted ones must specify a secure policy.
  LoadAndExpectError(
      "init_platform_app_csp_insecure.json",
      errors::kInsecureContentSecurityPolicy);
}

TEST_F(ExtensionManifestTest, CertainApisRequirePlatformApps) {
  // Put APIs here that should be restricted to platform apps, but that haven't
  // yet graduated from experimental.
  const char* kPlatformAppExperimentalApis[] = {
    "dns",
    "serial",
  };
  // TODO(miket): When the first platform-app API leaves experimental, write
  // similar code that tests without the experimental flag.

  // This manifest is a skeleton used to build more specific manifests for
  // testing. The requirements are that (1) it be a valid platform app, and (2)
  // it contain no permissions dictionary.
  std::string error;
  scoped_ptr<DictionaryValue> manifest(
      LoadManifestFile("init_valid_platform_app.json", &error));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create each manifest.
  for (size_t i = 0; i < arraysize(kPlatformAppExperimentalApis); ++i) {
    const char* api_name = kPlatformAppExperimentalApis[i];

    // DictionaryValue will take ownership of this ListValue.
    ListValue *permissions = new ListValue();
    permissions->Append(base::Value::CreateStringValue("experimental"));
    permissions->Append(base::Value::CreateStringValue(api_name));
    manifest->Set("permissions", permissions);

    // Each of these files lives in the scoped temp directory, so it will be
    // cleaned up at test teardown.
    base::FilePath file_path = temp_dir.path().AppendASCII(api_name);
    JSONFileValueSerializer serializer(file_path);
    serializer.Serialize(*(manifest.get()));
  }

  // First try to load without any flags. This should fail for every API.
  for (size_t i = 0; i < arraysize(kPlatformAppExperimentalApis); ++i) {
    const char* api_name = kPlatformAppExperimentalApis[i];
    base::FilePath file_path = temp_dir.path().AppendASCII(api_name);
    LoadAndExpectError(file_path.MaybeAsASCII().c_str(),
                       errors::kExperimentalFlagRequired);
  }

  // Now try again with the experimental flag set.
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);
  for (size_t i = 0; i < arraysize(kPlatformAppExperimentalApis); ++i) {
    const char* api_name = kPlatformAppExperimentalApis[i];
    base::FilePath file_path = temp_dir.path().AppendASCII(api_name);
    LoadAndExpectSuccess(file_path.MaybeAsASCII().c_str());
  }
}
