// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/config_dir_policy_provider.h"
#include "chrome/browser/mock_configuration_policy_store.h"
#include "chrome/common/json_value_serializer.h"
#include "testing/gtest/include/gtest/gtest.h"

class ConfigDirPolicyProviderTest : public testing::Test {
 protected:
  virtual void SetUp() {
    // Determine the directory to use for testing.
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_dir_));
    test_dir_ =
        test_dir_.Append(FILE_PATH_LITERAL("ConfigDirPolicyProviderTest"));

    // Make sure the directory is fresh.
    file_util::Delete(test_dir_, true);
    file_util::CreateDirectory(test_dir_);
    ASSERT_TRUE(file_util::DirectoryExists(test_dir_));

    // Create a fresh policy store mock.
    policy_store_.reset(new MockConfigurationPolicyStore());
  }

  virtual void TearDown() {
    // Clean up test directory.
    ASSERT_TRUE(file_util::Delete(test_dir_, true));
    ASSERT_FALSE(file_util::PathExists(test_dir_));
  }

  // JSON-encode a dictionary and write it to a file.
  void WriteConfigFile(const DictionaryValue& dict,
                       const std::string& file_name) {
    std::string data;
    JSONStringValueSerializer serializer(&data);
    serializer.Serialize(dict);
    FilePath file_path(test_dir_.AppendASCII(file_name));
    file_util::WriteFile(file_path, data.c_str(), data.size());
  }

  FilePath test_dir_;
  scoped_ptr<MockConfigurationPolicyStore> policy_store_;
};

// The preferences dictionary is expected to be empty when there are no files to
// load.
TEST_F(ConfigDirPolicyProviderTest, ReadPrefsEmpty) {
  ConfigDirPolicyProvider provider(test_dir_);
  EXPECT_TRUE(provider.Provide(policy_store_.get()));
  EXPECT_TRUE(policy_store_->policy_map().empty());
}

// Reading from a non-existent directory should result in an empty preferences
// dictionary.
TEST_F(ConfigDirPolicyProviderTest, ReadPrefsNonExistentDirectory) {
  FilePath non_existent_dir(test_dir_.Append(FILE_PATH_LITERAL("not_there")));
  ConfigDirPolicyProvider provider(non_existent_dir);
  EXPECT_TRUE(provider.Provide(policy_store_.get()));
  EXPECT_TRUE(policy_store_->policy_map().empty());
}

// Test reading back a single preference value.
TEST_F(ConfigDirPolicyProviderTest, ReadPrefsSinglePref) {
  DictionaryValue test_dict;
  test_dict.SetString(L"HomepageLocation", L"http://www.google.com");
  WriteConfigFile(test_dict, "config_file");
  ConfigDirPolicyProvider provider(test_dir_);

  EXPECT_TRUE(provider.Provide(policy_store_.get()));
  const MockConfigurationPolicyStore::PolicyMap& policy_map(
      policy_store_->policy_map());
  EXPECT_EQ(1U, policy_map.size());
  MockConfigurationPolicyStore::PolicyMap::const_iterator entry =
      policy_map.find(ConfigurationPolicyStore::kPolicyHomePage);
  ASSERT_TRUE(entry != policy_map.end());

  std::wstring str_value;
  EXPECT_TRUE(entry->second->GetAsString(&str_value));
  EXPECT_EQ(L"http://www.google.com", str_value);
}

// Test merging values from different files.
TEST_F(ConfigDirPolicyProviderTest, ReadPrefsMergePrefs) {
  // Write a bunch of data files in order to increase the chance to detect the
  // provider not respecting lexicographic ordering when reading them. Since the
  // filesystem may return files in arbitrary order, there is no way to be sure,
  // but this is better than nothing.
  DictionaryValue test_dict_bar;
  test_dict_bar.SetString(L"HomepageLocation", L"http://bar.com");
  for (unsigned int i = 1; i <= 4; ++i)
    WriteConfigFile(test_dict_bar, IntToString(i));
  DictionaryValue test_dict_foo;
  test_dict_foo.SetString(L"HomepageLocation", L"http://foo.com");
  WriteConfigFile(test_dict_foo, "9");
  for (unsigned int i = 5; i <= 8; ++i)
    WriteConfigFile(test_dict_bar, IntToString(i));
  ConfigDirPolicyProvider provider(test_dir_);

  EXPECT_TRUE(provider.Provide(policy_store_.get()));
  const MockConfigurationPolicyStore::PolicyMap& policy_map(
      policy_store_->policy_map());
  EXPECT_EQ(1U, policy_map.size());
  MockConfigurationPolicyStore::PolicyMap::const_iterator entry =
      policy_map.find(ConfigurationPolicyStore::kPolicyHomePage);
  ASSERT_TRUE(entry != policy_map.end());

  std::wstring str_value;
  EXPECT_TRUE(entry->second->GetAsString(&str_value));
  EXPECT_EQ(L"http://foo.com", str_value);
}
