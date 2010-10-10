// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/policy/config_dir_policy_provider.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/mock_configuration_policy_store.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;

namespace policy {

// Shorter reload intervals for testing PolicyDirWatcher.
const int kSettleIntervalSecondsForTesting = 0;
const int kReloadIntervalMinutesForTesting = 1;

template<typename BASE>
class ConfigDirPolicyProviderTestBase : public BASE {
 protected:
  ConfigDirPolicyProviderTestBase()
      : ui_thread_(BrowserThread::UI, &loop_),
        file_thread_(BrowserThread::FILE, &loop_) {}

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
  BrowserThread ui_thread_;
  BrowserThread file_thread_;
};

// A mock provider that allows us to capture reload notifications.
class MockConfigDirPolicyProvider : public ConfigDirPolicyProvider {
 public:
  explicit MockConfigDirPolicyProvider(const FilePath& config_dir_)
      : ConfigDirPolicyProvider(
          ConfigurationPolicyPrefStore::GetChromePolicyValueMap(),
          config_dir_) {
  }

  MOCK_METHOD0(NotifyStoreOfPolicyChange, void());
};

class PolicyDirLoaderTest
    : public ConfigDirPolicyProviderTestBase<testing::Test> {
 protected:
  PolicyDirLoaderTest() {}

  virtual void SetUp() {
    ConfigDirPolicyProviderTestBase<testing::Test>::SetUp();
    provider_.reset(new MockConfigDirPolicyProvider(test_dir_));
  }

  virtual void TearDown() {
    provider_.reset(NULL);
    ConfigDirPolicyProviderTestBase<testing::Test>::TearDown();
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

template<typename BASE>
class ConfigDirPolicyProviderTestWithMockStore
    : public ConfigDirPolicyProviderTestBase<BASE> {
 protected:
  virtual void SetUp() {
    ConfigDirPolicyProviderTestBase<BASE>::SetUp();
    // Create a fresh policy store mock.
    policy_store_.reset(new MockConfigurationPolicyStore());
  }

  scoped_ptr<MockConfigurationPolicyStore> policy_store_;
};

class ConfigDirPolicyProviderTest
    : public ConfigDirPolicyProviderTestWithMockStore<testing::Test> {
};

// The preferences dictionary is expected to be empty when there are no files to
// load.
TEST_F(ConfigDirPolicyProviderTest, ReadPrefsEmpty) {
  ConfigDirPolicyProvider provider(
      ConfigurationPolicyPrefStore::GetChromePolicyValueMap(), test_dir_);
  EXPECT_TRUE(provider.Provide(policy_store_.get()));
  EXPECT_TRUE(policy_store_->policy_map().empty());
}

// Reading from a non-existent directory should result in an empty preferences
// dictionary.
TEST_F(ConfigDirPolicyProviderTest, ReadPrefsNonExistentDirectory) {
  FilePath non_existent_dir(test_dir_.Append(FILE_PATH_LITERAL("not_there")));
  ConfigDirPolicyProvider provider(
      ConfigurationPolicyPrefStore::GetChromePolicyValueMap(),
      non_existent_dir);
  EXPECT_TRUE(provider.Provide(policy_store_.get()));
  EXPECT_TRUE(policy_store_->policy_map().empty());
}

// Test reading back a single preference value.
TEST_F(ConfigDirPolicyProviderTest, ReadPrefsSinglePref) {
  DictionaryValue test_dict;
  test_dict.SetString("HomepageLocation", "http://www.google.com");
  WriteConfigFile(test_dict, "config_file");
  ConfigDirPolicyProvider provider(
      ConfigurationPolicyPrefStore::GetChromePolicyValueMap(), test_dir_);

  EXPECT_TRUE(provider.Provide(policy_store_.get()));
  EXPECT_EQ(1U, policy_store_->policy_map().size());
  const Value* value =
      policy_store_->Get(ConfigurationPolicyStore::kPolicyHomePage);
  ASSERT_TRUE(value);
  std::string str_value;
  EXPECT_TRUE(value->GetAsString(&str_value));
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
  ConfigDirPolicyProvider provider(
      ConfigurationPolicyPrefStore::GetChromePolicyValueMap(), test_dir_);

  EXPECT_TRUE(provider.Provide(policy_store_.get()));
  EXPECT_EQ(1U, policy_store_->policy_map().size());
  const Value* value =
      policy_store_->Get(ConfigurationPolicyStore::kPolicyHomePage);
  ASSERT_TRUE(value);
  std::string str_value;
  EXPECT_TRUE(value->GetAsString(&str_value));
  EXPECT_EQ("http://foo.com", str_value);
}

// Holds policy type, corresponding policy key string and a valid value for use
// in parametrized value tests.
class ValueTestParams {
 public:
  // Assumes ownership of |test_value|.
  ValueTestParams(ConfigurationPolicyStore::PolicyType type,
                  const char* policy_key,
                  Value* test_value)
      : type_(type),
        policy_key_(policy_key),
        test_value_(test_value) {}

  // testing::TestWithParam does copying, so provide copy constructor and
  // assignment operator.
  ValueTestParams(const ValueTestParams& other)
      : type_(other.type_),
        policy_key_(other.policy_key_),
        test_value_(other.test_value_->DeepCopy()) {}

  const ValueTestParams& operator=(ValueTestParams other) {
    swap(other);
    return *this;
  }

  void swap(ValueTestParams& other) {
    std::swap(type_, other.type_);
    std::swap(policy_key_, other.policy_key_);
    test_value_.swap(other.test_value_);
  }

  ConfigurationPolicyStore::PolicyType type() const { return type_; }
  const char* policy_key() const { return policy_key_; }
  const Value* test_value() const { return test_value_.get(); }

  // Factory methods that create parameter objects for different value types.
  static ValueTestParams ForStringPolicy(
      ConfigurationPolicyStore::PolicyType type,
      const char* policy_key) {
    return ValueTestParams(type, policy_key, Value::CreateStringValue("test"));
  }
  static ValueTestParams ForBooleanPolicy(
      ConfigurationPolicyStore::PolicyType type,
      const char* policy_key) {
    return ValueTestParams(type, policy_key, Value::CreateBooleanValue(true));
  }
  static ValueTestParams ForIntegerPolicy(
      ConfigurationPolicyStore::PolicyType type,
      const char* policy_key) {
    return ValueTestParams(type, policy_key, Value::CreateIntegerValue(42));
  }
  static ValueTestParams ForListPolicy(
      ConfigurationPolicyStore::PolicyType type,
      const char* policy_key) {
    ListValue* value = new ListValue();
    value->Set(0U, Value::CreateStringValue("first"));
    value->Set(1U, Value::CreateStringValue("second"));
    return ValueTestParams(type, policy_key, value);
  }

 private:
  ConfigurationPolicyStore::PolicyType type_;
  const char* policy_key_;
  scoped_ptr<Value> test_value_;
};

// Tests whether the provider correctly reads a value from the file and forwards
// it to the store.
class ConfigDirPolicyProviderValueTest
    : public ConfigDirPolicyProviderTestWithMockStore<
          testing::TestWithParam<ValueTestParams> > {
};

TEST_P(ConfigDirPolicyProviderValueTest, Default) {
  ConfigDirPolicyProvider provider(
      ConfigurationPolicyPrefStore::GetChromePolicyValueMap(), test_dir_);
  EXPECT_TRUE(provider.Provide(policy_store_.get()));
  EXPECT_TRUE(policy_store_->policy_map().empty());
}

TEST_P(ConfigDirPolicyProviderValueTest, NullValue) {
  DictionaryValue dict;
  dict.Set(GetParam().policy_key(), Value::CreateNullValue());
  WriteConfigFile(dict, "empty");
  ConfigDirPolicyProvider provider(
      ConfigurationPolicyPrefStore::GetChromePolicyValueMap(), test_dir_);
  EXPECT_TRUE(provider.Provide(policy_store_.get()));
  EXPECT_TRUE(policy_store_->policy_map().empty());
}

TEST_P(ConfigDirPolicyProviderValueTest, TestValue) {
  DictionaryValue dict;
  dict.Set(GetParam().policy_key(), GetParam().test_value()->DeepCopy());
  WriteConfigFile(dict, "policy");
  ConfigDirPolicyProvider provider(
      ConfigurationPolicyPrefStore::GetChromePolicyValueMap(), test_dir_);
  EXPECT_TRUE(provider.Provide(policy_store_.get()));
  EXPECT_EQ(1U, policy_store_->policy_map().size());
  const Value* value = policy_store_->Get(GetParam().type());
  ASSERT_TRUE(value);
  EXPECT_TRUE(GetParam().test_value()->Equals(value));
}

// Test parameters for all supported policies.
INSTANTIATE_TEST_CASE_P(
    ConfigDirPolicyProviderValueTestInstance,
    ConfigDirPolicyProviderValueTest,
    testing::Values(
        ValueTestParams::ForStringPolicy(
            ConfigurationPolicyStore::kPolicyHomePage,
            key::kHomepageLocation),
        ValueTestParams::ForBooleanPolicy(
            ConfigurationPolicyStore::kPolicyHomepageIsNewTabPage,
            key::kHomepageIsNewTabPage),
        ValueTestParams::ForIntegerPolicy(
            ConfigurationPolicyStore::kPolicyRestoreOnStartup,
            key::kRestoreOnStartup),
        ValueTestParams::ForListPolicy(
            ConfigurationPolicyStore::kPolicyURLsToRestoreOnStartup,
            key::kURLsToRestoreOnStartup),
        ValueTestParams::ForBooleanPolicy(
            ConfigurationPolicyStore::kPolicyDefaultSearchProviderEnabled,
            key::kDefaultSearchProviderEnabled),
        ValueTestParams::ForStringPolicy(
            ConfigurationPolicyStore::kPolicyDefaultSearchProviderName,
            key::kDefaultSearchProviderName),
        ValueTestParams::ForStringPolicy(
            ConfigurationPolicyStore::kPolicyDefaultSearchProviderKeyword,
            key::kDefaultSearchProviderKeyword),
        ValueTestParams::ForStringPolicy(
            ConfigurationPolicyStore::kPolicyDefaultSearchProviderSearchURL,
            key::kDefaultSearchProviderSearchURL),
        ValueTestParams::ForStringPolicy(
            ConfigurationPolicyStore::kPolicyDefaultSearchProviderSuggestURL,
            key::kDefaultSearchProviderSuggestURL),
        ValueTestParams::ForStringPolicy(
            ConfigurationPolicyStore::kPolicyDefaultSearchProviderIconURL,
            key::kDefaultSearchProviderIconURL),
        ValueTestParams::ForStringPolicy(
            ConfigurationPolicyStore::kPolicyDefaultSearchProviderEncodings,
            key::kDefaultSearchProviderEncodings),
        ValueTestParams::ForIntegerPolicy(
            ConfigurationPolicyStore::kPolicyProxyServerMode,
            key::kProxyServerMode),
        ValueTestParams::ForStringPolicy(
            ConfigurationPolicyStore::kPolicyProxyServer,
            key::kProxyServer),
        ValueTestParams::ForStringPolicy(
            ConfigurationPolicyStore::kPolicyProxyPacUrl,
            key::kProxyPacUrl),
        ValueTestParams::ForStringPolicy(
            ConfigurationPolicyStore::kPolicyProxyBypassList,
            key::kProxyBypassList),
        ValueTestParams::ForBooleanPolicy(
            ConfigurationPolicyStore::kPolicyAlternateErrorPagesEnabled,
            key::kAlternateErrorPagesEnabled),
        ValueTestParams::ForBooleanPolicy(
            ConfigurationPolicyStore::kPolicySearchSuggestEnabled,
            key::kSearchSuggestEnabled),
        ValueTestParams::ForBooleanPolicy(
            ConfigurationPolicyStore::kPolicyDnsPrefetchingEnabled,
            key::kDnsPrefetchingEnabled),
        ValueTestParams::ForBooleanPolicy(
            ConfigurationPolicyStore::kPolicySafeBrowsingEnabled,
            key::kSafeBrowsingEnabled),
        ValueTestParams::ForBooleanPolicy(
            ConfigurationPolicyStore::kPolicyMetricsReportingEnabled,
            key::kMetricsReportingEnabled),
        ValueTestParams::ForBooleanPolicy(
            ConfigurationPolicyStore::kPolicyPasswordManagerEnabled,
            key::kPasswordManagerEnabled),
        ValueTestParams::ForBooleanPolicy(
            ConfigurationPolicyStore::kPolicyPasswordManagerAllowShowPasswords,
            key::kPasswordManagerAllowShowPasswords),
        ValueTestParams::ForListPolicy(
            ConfigurationPolicyStore::kPolicyDisabledPlugins,
            key::kDisabledPlugins),
        ValueTestParams::ForBooleanPolicy(
            ConfigurationPolicyStore::kPolicyAutoFillEnabled,
            key::kAutoFillEnabled),
        ValueTestParams::ForStringPolicy(
            ConfigurationPolicyStore::kPolicyApplicationLocale,
            key::kApplicationLocaleValue),
        ValueTestParams::ForBooleanPolicy(
            ConfigurationPolicyStore::kPolicySyncDisabled,
            key::kSyncDisabled),
        ValueTestParams::ForListPolicy(
            ConfigurationPolicyStore::kPolicyExtensionInstallAllowList,
            key::kExtensionInstallAllowList),
        ValueTestParams::ForListPolicy(
            ConfigurationPolicyStore::kPolicyExtensionInstallDenyList,
            key::kExtensionInstallDenyList),
        ValueTestParams::ForBooleanPolicy(
            ConfigurationPolicyStore::kPolicyShowHomeButton,
            key::kShowHomeButton),
        ValueTestParams::ForBooleanPolicy(
            ConfigurationPolicyStore::kPolicyPrintingEnabled,
            key::kPrintingEnabled)));

}  // namespace policy
