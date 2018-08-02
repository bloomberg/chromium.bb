// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/chrome_utils/extensions_util.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {
namespace {

const int kExtensionIdLength = 32;

struct TestRegistryEntry {
  HKEY hkey;
  const base::string16 path;
  const base::string16 name;
  const base::string16 value;
};

const TestRegistryEntry extension_forcelist_entries[] = {
    {HKEY_LOCAL_MACHINE,
     L"software\\policies\\google\\chrome\\ExtensionInstallForcelist", L"test1",
     L"ababababcdcdcdcdefefefefghghghgh;https://clients2.google.com/service/"
     L"update2/crx"},
    {HKEY_CURRENT_USER,
     L"software\\policies\\google\\chrome\\ExtensionInstallForcelist", L"test2",
     L"aaaabbbbccccddddeeeeffffgggghhhh;https://clients2.google.com/service/"
     L"update2/crx"}};

bool ExtensionPolicyFound(
    TestRegistryEntry test_entry,
    const std::vector<ExtensionRegistryPolicy>& found_policies) {
  for (const ExtensionRegistryPolicy& policy : found_policies) {
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
  for (const TestRegistryEntry& policy : extension_forcelist_entries) {
    base::win::RegKey policy_key;
    ASSERT_EQ(ERROR_SUCCESS, policy_key.Create(policy.hkey, policy.path.c_str(),
                                               KEY_ALL_ACCESS));
    DCHECK(policy_key.Valid());
    ASSERT_EQ(ERROR_SUCCESS,
              policy_key.WriteValue(policy.name.c_str(), policy.value.c_str()));
  }

  std::vector<ExtensionRegistryPolicy> policies;
  GetExtensionForcelistRegistryPolicies(&policies);

  for (const TestRegistryEntry& expected_result : extension_forcelist_entries) {
    EXPECT_TRUE(ExtensionPolicyFound(expected_result, policies));
  }
}

}  // namespace chrome_cleaner
