// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/chrome_utils/extensions_util.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/base_paths_win.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_reg_util_win.h"
#include "base/test/test_timeouts.h"
#include "base/win/registry.h"
#include "chrome/chrome_cleaner/parsers/json_parser/test_json_parser.h"
#include "chrome/chrome_cleaner/test/test_extensions.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::WaitableEvent;

namespace chrome_cleaner {
namespace {

const int kExtensionIdLength = 32;

struct ExtensionIDHash {
  size_t operator()(const ForceInstalledExtension& extension) const {
    return std::hash<std::string>{}(extension.id.AsString());
  }
};

struct ExtensionIDEqual {
  bool operator()(const ForceInstalledExtension& lhs,
                  const ForceInstalledExtension& rhs) const {
    return lhs.id == rhs.id;
  }
};

bool ExtensionPolicyRegistryEntryFound(
    TestRegistryEntry test_entry,
    const std::vector<ExtensionPolicyRegistryEntry>& found_policies) {
  for (const ExtensionPolicyRegistryEntry& policy : found_policies) {
    base::string16 test_entry_value(test_entry.value);
    if (policy.extension_id == test_entry_value.substr(0, kExtensionIdLength) &&
        policy.hkey == test_entry.hkey && policy.path == test_entry.path &&
        policy.name == test_entry.name) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST(ExtensionsUtilTest, GetExtensionForcelistRegistryPolicies) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_CURRENT_USER);
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);
  for (const TestRegistryEntry& policy : kExtensionForcelistEntries) {
    base::win::RegKey policy_key;
    ASSERT_EQ(ERROR_SUCCESS, policy_key.Create(policy.hkey, policy.path.c_str(),
                                               KEY_ALL_ACCESS));
    ASSERT_TRUE(policy_key.Valid());
    ASSERT_EQ(ERROR_SUCCESS,
              policy_key.WriteValue(policy.name.c_str(), policy.value.c_str()));
  }

  std::vector<ExtensionPolicyRegistryEntry> policies;
  GetExtensionForcelistRegistryPolicies(&policies);

  for (const TestRegistryEntry& expected_result : kExtensionForcelistEntries) {
    EXPECT_TRUE(ExtensionPolicyRegistryEntryFound(expected_result, policies));
  }
}

TEST(ExtensionsUtilTest, RemoveForcelistPolicyExtensions) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_CURRENT_USER);
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);
  for (const TestRegistryEntry& policy : kExtensionForcelistEntries) {
    base::win::RegKey policy_key;
    ASSERT_EQ(ERROR_SUCCESS, policy_key.Create(policy.hkey, policy.path.c_str(),
                                               KEY_ALL_ACCESS));
    DCHECK(policy_key.Valid());
    ASSERT_EQ(ERROR_SUCCESS,
              policy_key.WriteValue(policy.name.c_str(), policy.value.c_str()));
    base::string16 value;
    policy_key.ReadValue(policy.name.c_str(), &value);
    ASSERT_EQ(value, policy.value);
  }

  std::vector<ForceInstalledExtension> extensions;
  std::vector<ExtensionPolicyRegistryEntry> policies;
  GetExtensionForcelistRegistryPolicies(&policies);
  for (ExtensionPolicyRegistryEntry& policy : policies) {
    ForceInstalledExtension extension(
        ExtensionID::Create(base::UTF16ToUTF8(policy.extension_id)).value(),
        POLICY_EXTENSION_FORCELIST, "", "");
    extension.policy_registry_entry =
        std::make_shared<ExtensionPolicyRegistryEntry>(std::move(policy));
    extensions.push_back(extension);
  }

  for (ForceInstalledExtension& extension : extensions) {
    base::win::RegKey policy_key;
    ASSERT_TRUE(RemoveForcelistPolicyExtension(extension));
    ASSERT_EQ(ERROR_SUCCESS,
              policy_key.Open(extension.policy_registry_entry->hkey,
                              extension.policy_registry_entry->path.c_str(),
                              KEY_READ));
    base::string16 value;
    policy_key.ReadValue(extension.policy_registry_entry->name.c_str(), &value);
    ASSERT_EQ(value, L"");
  }
}

TEST(ExtensionsUtilTest, GetNonWhitelistedDefaultExtensions) {
  // Set up a fake default extensions JSON file.
  base::ScopedPathOverride program_files_override(base::DIR_PROGRAM_FILES);
  base::FilePath program_files_dir;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_PROGRAM_FILES, &program_files_dir));

  base::FilePath fake_apps_dir(
      program_files_dir.Append(kFakeChromeFolder).Append(L"default_apps"));
  ASSERT_TRUE(base::CreateDirectoryAndGetError(fake_apps_dir, nullptr));

  base::FilePath default_extensions_file =
      fake_apps_dir.Append(L"external_extensions.json");
  CreateFileWithContent(default_extensions_file, kDefaultExtensionsJson,
                        sizeof(kDefaultExtensionsJson) - 1);
  ASSERT_TRUE(base::PathExists(default_extensions_file));

  // Set up an invalid default extensions JSON file
  base::ScopedPathOverride program_files_x86_override(
      base::DIR_PROGRAM_FILESX86);
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_PROGRAM_FILESX86, &program_files_dir));

  fake_apps_dir =
      program_files_dir.Append(kFakeChromeFolder).Append(L"default_apps");
  ASSERT_TRUE(base::CreateDirectoryAndGetError(fake_apps_dir, nullptr));

  default_extensions_file = fake_apps_dir.Append(L"external_extensions.json");
  CreateFileWithContent(default_extensions_file, kInvalidDefaultExtensionsJson,
                        sizeof(kInvalidDefaultExtensionsJson) - 1);
  ASSERT_TRUE(base::PathExists(default_extensions_file));

  TestJsonParser json_parser;
  std::vector<ExtensionPolicyFile> policies;
  base::WaitableEvent done(WaitableEvent::ResetPolicy::MANUAL,
                           WaitableEvent::InitialState::NOT_SIGNALED);
  GetNonWhitelistedDefaultExtensions(&json_parser, &policies, &done);
  ASSERT_TRUE(done.TimedWait(TestTimeouts::action_timeout()));

  const base::string16 expected_extension_ids[] = {kTestExtensionId1,
                                                   kTestExtensionId2};
  ASSERT_EQ(base::size(expected_extension_ids), policies.size());
  const base::string16 found_extension_ids[] = {policies[0].extension_id,
                                                policies[1].extension_id};
  EXPECT_THAT(expected_extension_ids,
              ::testing::UnorderedElementsAreArray(found_extension_ids));
}

TEST(ExtensionsUtilTest, RemoveNonWhitelistedDefaultExtensions) {
  // Set up a fake default extensions JSON file.
  base::ScopedPathOverride program_files_override(base::DIR_PROGRAM_FILES);
  base::FilePath program_files_dir;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_PROGRAM_FILES, &program_files_dir));

  base::FilePath fake_apps_dir(
      program_files_dir.Append(kFakeChromeFolder).Append(L"default_apps"));
  ASSERT_TRUE(base::CreateDirectoryAndGetError(fake_apps_dir, nullptr));

  base::FilePath default_extensions_file =
      fake_apps_dir.Append(L"external_extensions.json");
  CreateFileWithContent(default_extensions_file, kDefaultExtensionsJson,
                        sizeof(kDefaultExtensionsJson) - 1);
  ASSERT_TRUE(base::PathExists(default_extensions_file));

  // Set up an invalid default extensions JSON file
  base::ScopedPathOverride program_files_x86_override(
      base::DIR_PROGRAM_FILESX86);
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_PROGRAM_FILESX86, &program_files_dir));

  fake_apps_dir =
      program_files_dir.Append(kFakeChromeFolder).Append(L"default_apps");
  ASSERT_TRUE(base::CreateDirectoryAndGetError(fake_apps_dir, nullptr));

  default_extensions_file = fake_apps_dir.Append(L"external_extensions.json");
  CreateFileWithContent(default_extensions_file, kInvalidDefaultExtensionsJson,
                        sizeof(kInvalidDefaultExtensionsJson) - 1);
  ASSERT_TRUE(base::PathExists(default_extensions_file));

  TestJsonParser json_parser;
  std::vector<ExtensionPolicyFile> policies;
  base::WaitableEvent done(WaitableEvent::ResetPolicy::MANUAL,
                           WaitableEvent::InitialState::NOT_SIGNALED);
  GetNonWhitelistedDefaultExtensions(&json_parser, &policies, &done);
  ASSERT_TRUE(done.TimedWait(TestTimeouts::action_timeout()));

  std::vector<ForceInstalledExtension> extensions;
  for (ExtensionPolicyFile& policy : policies) {
    ForceInstalledExtension extension(
        ExtensionID::Create(base::UTF16ToUTF8(policy.extension_id)).value(),
        DEFAULT_APPS_EXTENSION, "", "");
    extension.policy_file =
        std::make_shared<ExtensionPolicyFile>(std::move(policy));
    extensions.push_back(extension);
  }
  base::Value json_result = extensions[0].policy_file->json->data.Clone();
  for (ForceInstalledExtension& extension : extensions) {
    ASSERT_TRUE(RemoveDefaultExtension(extension, &json_result));
  }
  std::string result;
  JSONStringValueSerializer serializer(&result);
  ASSERT_TRUE(serializer.Serialize(json_result));
  ASSERT_EQ(result, kValidDefaultExtensionsJson);
  base::Value original = json_result.Clone();
  for (ForceInstalledExtension& extension : extensions) {
    ASSERT_FALSE(RemoveDefaultExtension(extension, &json_result));
    ASSERT_EQ(original, json_result);
  }
}

TEST(ExtensionsUtilTest, GetNonWhitelistedDefaultExtensionsNoFilesFound) {
  base::ScopedPathOverride program_files_override(base::DIR_PROGRAM_FILES);
  base::ScopedPathOverride program_files_x86_override(
      base::DIR_PROGRAM_FILESX86);

  TestJsonParser json_parser;
  std::vector<ExtensionPolicyFile> policies;
  base::WaitableEvent done(WaitableEvent::ResetPolicy::MANUAL,
                           WaitableEvent::InitialState::NOT_SIGNALED);
  GetNonWhitelistedDefaultExtensions(&json_parser, &policies, &done);
  ASSERT_TRUE(done.TimedWait(TestTimeouts::action_timeout()));

  size_t expected_policies_found = 0;
  ASSERT_EQ(expected_policies_found, policies.size());
}

TEST(ExtensionsUtilTest, GetExtensionSettingsForceInstalledExtensions) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_CURRENT_USER);
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  base::win::RegKey settings_key;
  ASSERT_EQ(ERROR_SUCCESS,
            settings_key.Create(HKEY_LOCAL_MACHINE,
                                kExtensionSettingsPolicyPath, KEY_ALL_ACCESS));
  ASSERT_TRUE(settings_key.Valid());
  ASSERT_EQ(ERROR_SUCCESS,
            settings_key.WriteValue(kExtensionSettingsName,
                                    kExtensionSettingsJsonOnlyForced));

  TestJsonParser json_parser;
  std::vector<ExtensionPolicyRegistryEntry> policies;
  base::WaitableEvent done(WaitableEvent::ResetPolicy::MANUAL,
                           WaitableEvent::InitialState::NOT_SIGNALED);
  GetExtensionSettingsForceInstalledExtensions(&json_parser, &policies, &done);
  ASSERT_TRUE(done.TimedWait(TestTimeouts::action_timeout()));

  // Check that only the two force installed extensions were found
  const base::string16 expected_extension_ids[] = {kTestExtensionId4,
                                                   kTestExtensionId5};
  const base::string16 found_extension_ids[] = {policies[0].extension_id,
                                                policies[1].extension_id};
  EXPECT_THAT(expected_extension_ids,
              ::testing::UnorderedElementsAreArray(found_extension_ids));

  // Also check that the collected registry entries match the values in the
  // registry.
  for (const ExtensionPolicyRegistryEntry& policy : policies) {
    EXPECT_EQ(policy.hkey, HKEY_LOCAL_MACHINE);
    EXPECT_EQ(policy.path, kExtensionSettingsPolicyPath);
    EXPECT_EQ(policy.name, kExtensionSettingsName);
  }
}

TEST(ExtensionsUtilTest, RemoveExtensionSettingsForceInstalledExtensions) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_CURRENT_USER);
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  base::win::RegKey settings_key;
  ASSERT_EQ(ERROR_SUCCESS,
            settings_key.Create(HKEY_LOCAL_MACHINE,
                                kExtensionSettingsPolicyPath, KEY_ALL_ACCESS));
  DCHECK(settings_key.Valid());
  ASSERT_EQ(ERROR_SUCCESS,
            settings_key.WriteValue(kExtensionSettingsName,
                                    kExtensionSettingsJsonOnlyForced));

  TestJsonParser json_parser;
  std::vector<ExtensionPolicyRegistryEntry> policies;
  base::WaitableEvent done(WaitableEvent::ResetPolicy::MANUAL,
                           WaitableEvent::InitialState::NOT_SIGNALED);
  GetExtensionSettingsForceInstalledExtensions(&json_parser, &policies, &done);
  ASSERT_TRUE(done.TimedWait(TestTimeouts::action_timeout()));

  std::unordered_set<ForceInstalledExtension, ExtensionIDHash, ExtensionIDEqual>
      extensions;
  base::Value json_result = policies[0].json->data.Clone();
  for (ExtensionPolicyRegistryEntry& policy : policies) {
    ForceInstalledExtension extension(
        ExtensionID::Create(base::UTF16ToUTF8(policy.extension_id)).value(),
        POLICY_EXTENSION_SETTINGS, "", "");
    extension.policy_registry_entry =
        std::make_shared<ExtensionPolicyRegistryEntry>(std::move(policy));
    extensions.insert(extension);
  }

  for (const ForceInstalledExtension& extension : extensions) {
    ASSERT_TRUE(
        RemoveExtensionSettingsPoliciesExtension(extension, &json_result));
  }
  std::string result;
  JSONStringValueSerializer serializer(&result);
  ASSERT_TRUE(serializer.Serialize(json_result));
  ASSERT_EQ(result, "{}");
  base::Value original = json_result.Clone();
  for (const ForceInstalledExtension& extension : extensions) {
    ASSERT_FALSE(
        RemoveExtensionSettingsPoliciesExtension(extension, &json_result));
    ASSERT_EQ(original, json_result);
  }
}

TEST(ExtensionsUtilTest, RemoveSomeExtensionSettingsForceInstalledExtensions) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_CURRENT_USER);
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  base::win::RegKey settings_key;
  ASSERT_EQ(ERROR_SUCCESS,
            settings_key.Create(HKEY_LOCAL_MACHINE,
                                kExtensionSettingsPolicyPath, KEY_ALL_ACCESS));
  DCHECK(settings_key.Valid());
  ASSERT_EQ(ERROR_SUCCESS, settings_key.WriteValue(kExtensionSettingsName,
                                                   kExtensionSettingsJson));

  TestJsonParser json_parser;
  std::vector<ExtensionPolicyRegistryEntry> policies;
  base::WaitableEvent done(WaitableEvent::ResetPolicy::MANUAL,
                           WaitableEvent::InitialState::NOT_SIGNALED);
  GetExtensionSettingsForceInstalledExtensions(&json_parser, &policies, &done);
  ASSERT_TRUE(done.TimedWait(TestTimeouts::action_timeout()));

  std::unordered_set<ForceInstalledExtension, ExtensionIDHash, ExtensionIDEqual>
      extensions;
  base::Value json_result = policies[0].json->data.Clone();
  for (ExtensionPolicyRegistryEntry& policy : policies) {
    ForceInstalledExtension extension(
        ExtensionID::Create(base::UTF16ToUTF8(policy.extension_id)).value(),
        POLICY_EXTENSION_SETTINGS, "", "");
    extension.policy_registry_entry =
        std::make_shared<ExtensionPolicyRegistryEntry>(std::move(policy));
    extensions.insert(extension);
  }

  for (const ForceInstalledExtension& extension : extensions) {
    ASSERT_TRUE(
        RemoveExtensionSettingsPoliciesExtension(extension, &json_result));
  }
  std::string result;
  JSONStringValueSerializer serializer(&result);
  ASSERT_TRUE(serializer.Serialize(json_result));
  ASSERT_EQ(result, kValidExtensionSettingsJson);
  base::Value original = json_result.Clone();
  for (const ForceInstalledExtension& extension : extensions) {
    ASSERT_FALSE(
        RemoveExtensionSettingsPoliciesExtension(extension, &json_result));
    ASSERT_EQ(original, json_result);
  }
}

TEST(ExtensionsUtilTest,
     GetExtensionSettingsForceInstalledExtensionsNoneFound) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_CURRENT_USER);
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  TestJsonParser json_parser;
  std::vector<ExtensionPolicyRegistryEntry> policies;
  base::WaitableEvent done(WaitableEvent::ResetPolicy::MANUAL,
                           WaitableEvent::InitialState::NOT_SIGNALED);
  GetExtensionSettingsForceInstalledExtensions(&json_parser, &policies, &done);
  ASSERT_TRUE(done.TimedWait(TestTimeouts::action_timeout()));

  size_t expected_policies_found = 0;
  ASSERT_EQ(expected_policies_found, policies.size());
}

TEST(ExtensionsUtilTest, GetMasterPreferencesExtensions) {
  // Set up a fake master preferences JSON file.
  base::ScopedPathOverride program_files_override(base::DIR_PROGRAM_FILES);
  base::FilePath program_files_dir;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_PROGRAM_FILES, &program_files_dir));

  base::FilePath chrome_dir(program_files_dir.Append(kChromeExePath));
  ASSERT_TRUE(base::CreateDirectoryAndGetError(chrome_dir, nullptr));

  base::FilePath master_preferences =
      chrome_dir.Append(kMasterPreferencesFileName);
  CreateFileWithContent(master_preferences, kMasterPreferencesJson,
                        sizeof(kMasterPreferencesJson) - 1);
  ASSERT_TRUE(base::PathExists(master_preferences));

  TestJsonParser json_parser;
  std::vector<ExtensionPolicyFile> policies;
  base::WaitableEvent done(WaitableEvent::ResetPolicy::MANUAL,
                           WaitableEvent::InitialState::NOT_SIGNALED);
  GetMasterPreferencesExtensions(&json_parser, &policies, &done);
  ASSERT_TRUE(done.TimedWait(TestTimeouts::action_timeout()));

  const base::string16 expected_extension_ids[] = {kTestExtensionId6,
                                                   kTestExtensionId7};
  ASSERT_EQ(base::size(expected_extension_ids), policies.size());
  const base::string16 found_extension_ids[] = {policies[0].extension_id,
                                                policies[1].extension_id};
  EXPECT_THAT(expected_extension_ids,
              ::testing::UnorderedElementsAreArray(found_extension_ids));
}

TEST(ExtensionsUtilTest, RemoveMasterPreferencesExtensionsNoneFound) {
  // Set up a fake master preferences JSON file.
  base::ScopedPathOverride program_files_override(base::DIR_PROGRAM_FILES);
  base::FilePath program_files_dir;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_PROGRAM_FILES, &program_files_dir));

  base::FilePath chrome_dir(program_files_dir.Append(kChromeExePath));
  ASSERT_TRUE(base::CreateDirectoryAndGetError(chrome_dir, nullptr));

  base::FilePath master_preferences =
      chrome_dir.Append(kMasterPreferencesFileName);
  CreateFileWithContent(master_preferences, kMasterPreferencesJson,
                        sizeof(kMasterPreferencesJson) - 1);
  ASSERT_TRUE(base::PathExists(master_preferences));

  TestJsonParser json_parser;
  std::vector<ExtensionPolicyFile> policies;
  base::WaitableEvent done(WaitableEvent::ResetPolicy::MANUAL,
                           WaitableEvent::InitialState::NOT_SIGNALED);
  GetMasterPreferencesExtensions(&json_parser, &policies, &done);
  ASSERT_TRUE(done.TimedWait(TestTimeouts::action_timeout()));

  std::vector<ForceInstalledExtension> extensions;
  for (ExtensionPolicyFile& policy : policies) {
    ForceInstalledExtension extension(
        ExtensionID::Create(base::UTF16ToUTF8(policy.extension_id)).value(),
        POLICY_MASTER_PREFERENCES, "", "");
    extension.policy_file =
        std::make_shared<ExtensionPolicyFile>(std::move(policy));
    extensions.push_back(extension);
  }
  base::Value json_result = extensions[0].policy_file->json->data.Clone();
  for (ForceInstalledExtension& extension : extensions) {
    ASSERT_TRUE(RemoveMasterPreferencesExtension(extension, &json_result));
  }
  std::string result;
  JSONStringValueSerializer serializer(&result);
  ASSERT_TRUE(serializer.Serialize(json_result));
  ASSERT_EQ(result, kValidMasterPreferencesJson);
}

TEST(ExtensionsUtilTest, GetMasterPreferencesExtensionsNoneFound) {
  // Set up a fake master preferences JSON file.
  base::ScopedPathOverride program_files_override(base::DIR_PROGRAM_FILES);
  base::FilePath program_files_dir;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_PROGRAM_FILES, &program_files_dir));

  base::FilePath chrome_dir(program_files_dir.Append(kChromeExePath));
  ASSERT_TRUE(base::CreateDirectoryAndGetError(chrome_dir, nullptr));

  base::FilePath master_preferences =
      chrome_dir.Append(kMasterPreferencesFileName);
  CreateFileWithContent(master_preferences, kMasterPreferencesJsonNoExtensions,
                        sizeof(kMasterPreferencesJsonNoExtensions) - 1);
  ASSERT_TRUE(base::PathExists(master_preferences));

  TestJsonParser json_parser;
  std::vector<ExtensionPolicyFile> policies;
  base::WaitableEvent done(WaitableEvent::ResetPolicy::MANUAL,
                           WaitableEvent::InitialState::NOT_SIGNALED);
  GetMasterPreferencesExtensions(&json_parser, &policies, &done);
  ASSERT_TRUE(done.TimedWait(TestTimeouts::action_timeout()));

  size_t expected_policies_found = 0;
  ASSERT_EQ(expected_policies_found, policies.size());
}

}  // namespace chrome_cleaner
