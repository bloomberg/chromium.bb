// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

#include "base/command_line.h"
#include "base/json/json_file_value_serializer.h"
#include "base/scoped_temp_dir.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extension_manifest_errors;

TEST_F(ExtensionManifestTest, PlatformApps) {
  // A minimal platform app.
  LoadAndExpectError("init_valid_platform_app.json",
                     errors::kPlatformAppFlagRequired);

  CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnablePlatformApps);

  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("init_valid_platform_app.json");
  EXPECT_TRUE(extension->is_storage_isolated());

  extension = LoadAndExpectSuccess("init_invalid_platform_app_1.json");
  ASSERT_TRUE(extension);
  ASSERT_EQ(1u, extension->install_warnings().size());
  EXPECT_EQ("'app.launch' is not allowed for specified package type "
                "(theme, app, etc.).",
            extension->install_warnings()[0]);

  LoadAndExpectError(
      "init_invalid_platform_app_2.json",
      extension_manifest_errors::kBackgroundRequiredForPlatformApps);

  LoadAndExpectError(
      "init_invalid_platform_app_3.json",
      extension_manifest_errors::kPlatformAppNeedsManifestVersion2);
}

TEST_F(ExtensionManifestTest, CertainApisRequirePlatformApps) {
  // Put APIs here that should be restricted to platform apps, but that haven't
  // yet graduated from experimental.
  const char* kPlatformAppExperimentalApis[] = {
    "dns",
    "serial",
    "socket",
  };
  // TODO(miket): When the first platform-app API leaves experimental, write
  // similar code that tests without the experimental flag.

  // This manifest is a skeleton used to build more specific manifests for
  // testing. The requirements are that (1) it be a valid platform app, and (2)
  // it contain no permissions dictionary.
  std::string error;
  scoped_ptr<DictionaryValue> manifest(
      LoadManifestFile("init_valid_platform_app.json", &error));

  ScopedTempDir temp_dir;
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
    FilePath file_path = temp_dir.path().AppendASCII(api_name);
    JSONFileValueSerializer serializer(file_path);
    serializer.Serialize(*(manifest.get()));
  }

  // First try to load without any flags. This should fail for every API.
  for (size_t i = 0; i < arraysize(kPlatformAppExperimentalApis); ++i) {
    const char* api_name = kPlatformAppExperimentalApis[i];
    FilePath file_path = temp_dir.path().AppendASCII(api_name);
    LoadAndExpectError(file_path.MaybeAsASCII().c_str(),
                       errors::kExperimentalFlagRequired);
  }

  // Now try again with experimental flag set.
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);
  for (size_t i = 0; i < arraysize(kPlatformAppExperimentalApis); ++i) {
    const char* api_name = kPlatformAppExperimentalApis[i];
    FilePath file_path = temp_dir.path().AppendASCII(api_name);
    LoadAndExpectError(file_path.MaybeAsASCII().c_str(),
                       errors::kPlatformAppFlagRequired);
  }

  // Finally, we should succeed with both experimental and platform-app flags
  // set.
  CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnablePlatformApps);
  for (size_t i = 0; i < arraysize(kPlatformAppExperimentalApis); ++i) {
    const char* api_name = kPlatformAppExperimentalApis[i];
    FilePath file_path = temp_dir.path().AppendASCII(api_name);
    LoadAndExpectSuccess(file_path.MaybeAsASCII().c_str());
  }
}
