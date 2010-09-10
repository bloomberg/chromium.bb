// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/policy/config_dir_policy_provider.h"
#include "chrome/browser/policy/mock_configuration_policy_store.h"
#include "chrome/common/json_value_serializer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;

namespace {

// Shorter reload intervals for testing PolicyDirWatcher.
const int kSettleIntervalSecondsForTesting = 0;
const int kReloadIntervalMinutesForTesting = 1;

}  // namespace

class ConfigDirPolicyProviderTestBase : public testing::Test {
 protected:
  ConfigDirPolicyProviderTestBase()
      : ui_thread_(ChromeThread::UI, &loop_),
        file_thread_(ChromeThread::FILE, &loop_) {}

  virtual void SetUp() {
    // Determine the directory to use for testing.
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_dir_));
    test_dir_ =
        test_dir_.Append(FILE_PATH_LITERAL("ConfigDirPolicyProviderTest"));

    // Make sure the directory is fresh.
    file_util::Delete(test_dir_, true);
    file_util::CreateDirectory(test_dir_);
    ASSERT_TRUE(file_util::DirectoryExists(test_dir_));
  }

  virtual void TearDown() {
    loop_.RunAllPending();
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
    ASSERT_TRUE(file_util::WriteFile(file_path, data.c_str(), data.size()));
  }

  FilePath test_dir_;
  MessageLoop loop_;
  ChromeThread ui_thread_;
  ChromeThread file_thread_;
};

// A mock provider that allows us to capture to reload notifications.
class MockConfigDirPolicyProvider : public ConfigDirPolicyProvider {
 public:
  explicit MockConfigDirPolicyProvider(const FilePath& config_dir_)
      : ConfigDirPolicyProvider(config_dir_) {}

  MOCK_METHOD0(NotifyStoreOfPolicyChange, void());
};

class PolicyDirLoaderTest : public ConfigDirPolicyProviderTestBase {
 protected:
  PolicyDirLoaderTest() {}

  virtual void SetUp() {
    ConfigDirPolicyProviderTestBase::SetUp();
    provider_.reset(new MockConfigDirPolicyProvider(test_dir_));
  }

  virtual void TearDown() {
    provider_.reset(NULL);
    ConfigDirPolicyProviderTestBase::TearDown();
  }

  scoped_ptr<MockConfigDirPolicyProvider> provider_;
};

TEST_F(PolicyDirLoaderTest, BasicLoad) {
  DictionaryValue test_dict;
  test_dict.SetString("HomepageLocation", "http://www.google.com");
  WriteConfigFile(test_dict, "config_file");

  scoped_refptr<PolicyDirLoader> loader_(
      new PolicyDirLoader(provider_->AsWeakPtr(), test_dir_,
                          kSettleIntervalSecondsForTesting,
                          kReloadIntervalMinutesForTesting));
  scoped_ptr<DictionaryValue> policy(loader_->GetPolicy());
  EXPECT_TRUE(policy.get());
  EXPECT_EQ(1U, policy->size());

  std::string str_value;
  EXPECT_TRUE(policy->GetString("HomepageLocation", &str_value));
  EXPECT_EQ("http://www.google.com", str_value);

  loader_->Stop();
}

TEST_F(PolicyDirLoaderTest, TestRefresh) {
  scoped_refptr<PolicyDirLoader> loader_(
      new PolicyDirLoader(provider_->AsWeakPtr(), test_dir_,
                          kSettleIntervalSecondsForTesting,
                          kReloadIntervalMinutesForTesting));
  scoped_ptr<DictionaryValue> policy(loader_->GetPolicy());
  EXPECT_TRUE(policy.get());
  EXPECT_EQ(0U, policy->size());

  DictionaryValue test_dict;
  test_dict.SetString("HomepageLocation", "http://www.google.com");
  WriteConfigFile(test_dict, "config_file");

  EXPECT_CALL(*provider_, NotifyStoreOfPolicyChange()).Times(1);
  loader_->OnFilePathChanged(test_dir_.AppendASCII("config_file"));

  // Run the loop. The refresh should be handled immediately since the settle
  // interval has been disabled.
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(provider_.get());

  policy.reset(loader_->GetPolicy());
  EXPECT_TRUE(policy.get());
  EXPECT_EQ(1U, policy->size());

  std::string str_value;
  EXPECT_TRUE(policy->GetString("HomepageLocation", &str_value));
  EXPECT_EQ("http://www.google.com", str_value);

  loader_->Stop();
}

class ConfigDirPolicyProviderTest : public ConfigDirPolicyProviderTestBase {
 protected:
  virtual void SetUp() {
    ConfigDirPolicyProviderTestBase::SetUp();
    // Create a fresh policy store mock.
    policy_store_.reset(new MockConfigurationPolicyStore());
  }

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
  test_dict.SetString("HomepageLocation", "http://www.google.com");
  WriteConfigFile(test_dict, "config_file");
  ConfigDirPolicyProvider provider(test_dir_);

  EXPECT_TRUE(provider.Provide(policy_store_.get()));
  const MockConfigurationPolicyStore::PolicyMap& policy_map(
      policy_store_->policy_map());
  EXPECT_EQ(1U, policy_map.size());
  MockConfigurationPolicyStore::PolicyMap::const_iterator entry =
      policy_map.find(ConfigurationPolicyStore::kPolicyHomePage);
  ASSERT_TRUE(entry != policy_map.end());

  std::string str_value;
  EXPECT_TRUE(entry->second->GetAsString(&str_value));
  EXPECT_EQ("http://www.google.com", str_value);
}

// Test merging values from different files.
TEST_F(ConfigDirPolicyProviderTest, ReadPrefsMergePrefs) {
  // Write a bunch of data files in order to increase the chance to detect the
  // provider not respecting lexicographic ordering when reading them. Since the
  // filesystem may return files in arbitrary order, there is no way to be sure,
  // but this is better than nothing.
  DictionaryValue test_dict_bar;
  test_dict_bar.SetString("HomepageLocation", "http://bar.com");
  for (unsigned int i = 1; i <= 4; ++i)
    WriteConfigFile(test_dict_bar, base::IntToString(i));
  DictionaryValue test_dict_foo;
  test_dict_foo.SetString("HomepageLocation", "http://foo.com");
  WriteConfigFile(test_dict_foo, "9");
  for (unsigned int i = 5; i <= 8; ++i)
    WriteConfigFile(test_dict_bar, base::IntToString(i));
  ConfigDirPolicyProvider provider(test_dir_);

  EXPECT_TRUE(provider.Provide(policy_store_.get()));
  const MockConfigurationPolicyStore::PolicyMap& policy_map(
      policy_store_->policy_map());
  EXPECT_EQ(1U, policy_map.size());
  MockConfigurationPolicyStore::PolicyMap::const_iterator entry =
      policy_map.find(ConfigurationPolicyStore::kPolicyHomePage);
  ASSERT_TRUE(entry != policy_map.end());

  std::string str_value;
  EXPECT_TRUE(entry->second->GetAsString(&str_value));
  EXPECT_EQ("http://foo.com", str_value);
}
