// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/policy/config_dir_policy_provider.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/mock_configuration_policy_store.h"
#include "chrome/common/json_value_serializer.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

template<typename BASE>
class ConfigDirPolicyProviderTestBase : public BASE {
 protected:
  ConfigDirPolicyProviderTestBase() {}

  virtual void SetUp() {
    ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
  }

  // JSON-encode a dictionary and write it to a file.
  void WriteConfigFile(const DictionaryValue& dict,
                       const std::string& file_name) {
    std::string data;
    JSONStringValueSerializer serializer(&data);
    serializer.Serialize(dict);
    const FilePath file_path(test_dir().AppendASCII(file_name));
    ASSERT_TRUE(file_util::WriteFile(file_path, data.c_str(), data.size()));
  }

  const FilePath& test_dir() { return test_dir_.path(); }

 private:
  ScopedTempDir test_dir_;
};

class ConfigDirPolicyLoaderTest
    : public ConfigDirPolicyProviderTestBase<testing::Test> {
};

// The preferences dictionary is expected to be empty when there are no files to
// load.
TEST_F(ConfigDirPolicyLoaderTest, ReadPrefsEmpty) {
  ConfigDirPolicyProviderDelegate loader(test_dir());
  scoped_ptr<DictionaryValue> policy(loader.Load());
  EXPECT_TRUE(policy.get());
  EXPECT_TRUE(policy->empty());
}

// Reading from a non-existent directory should result in an empty preferences
// dictionary.
TEST_F(ConfigDirPolicyLoaderTest, ReadPrefsNonExistentDirectory) {
  FilePath non_existent_dir(test_dir().Append(FILE_PATH_LITERAL("not_there")));
  ConfigDirPolicyProviderDelegate loader(non_existent_dir);
  scoped_ptr<DictionaryValue> policy(loader.Load());
  EXPECT_TRUE(policy.get());
  EXPECT_TRUE(policy->empty());
}

// Test reading back a single preference value.
TEST_F(ConfigDirPolicyLoaderTest, ReadPrefsSinglePref) {
  DictionaryValue test_dict;
  test_dict.SetString("HomepageLocation", "http://www.google.com");
  WriteConfigFile(test_dict, "config_file");

  ConfigDirPolicyProviderDelegate loader(test_dir());
  scoped_ptr<DictionaryValue> policy(loader.Load());
  EXPECT_TRUE(policy.get());
  EXPECT_TRUE(policy->Equals(&test_dict));
}

// Test merging values from different files.
TEST_F(ConfigDirPolicyLoaderTest, ReadPrefsMergePrefs) {
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

  ConfigDirPolicyProviderDelegate loader(test_dir());
  scoped_ptr<DictionaryValue> policy(loader.Load());
  EXPECT_TRUE(policy.get());
  EXPECT_TRUE(policy->Equals(&test_dict_foo));
}

// Holds policy type, corresponding policy key string and a valid value for use
// in parametrized value tests.
class ValueTestParams {
 public:
  // Assumes ownership of |test_value|.
  ValueTestParams(ConfigurationPolicyType type,
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

  ConfigurationPolicyType type() const { return type_; }
  const char* policy_key() const { return policy_key_; }
  const Value* test_value() const { return test_value_.get(); }

  // Factory methods that create parameter objects for different value types.
  static ValueTestParams ForStringPolicy(
      ConfigurationPolicyType type,
      const char* policy_key) {
    return ValueTestParams(type, policy_key, Value::CreateStringValue("test"));
  }
  static ValueTestParams ForBooleanPolicy(
      ConfigurationPolicyType type,
      const char* policy_key) {
    return ValueTestParams(type, policy_key, Value::CreateBooleanValue(true));
  }
  static ValueTestParams ForIntegerPolicy(
      ConfigurationPolicyType type,
      const char* policy_key) {
    return ValueTestParams(type, policy_key, Value::CreateIntegerValue(42));
  }
  static ValueTestParams ForListPolicy(
      ConfigurationPolicyType type,
      const char* policy_key) {
    ListValue* value = new ListValue();
    value->Set(0U, Value::CreateStringValue("first"));
    value->Set(1U, Value::CreateStringValue("second"));
    return ValueTestParams(type, policy_key, value);
  }

 private:
  ConfigurationPolicyType type_;
  const char* policy_key_;
  scoped_ptr<Value> test_value_;
};

// Tests whether the provider correctly reads a value from the file and forwards
// it to the store.
class ConfigDirPolicyProviderValueTest
    : public ConfigDirPolicyProviderTestBase<
          testing::TestWithParam<ValueTestParams> > {
 protected:
  ConfigDirPolicyProviderValueTest()
      : ui_thread_(BrowserThread::UI, &loop_),
        file_thread_(BrowserThread::FILE, &loop_) {}

  virtual void TearDown() {
    loop_.RunAllPending();
  }

  MockConfigurationPolicyStore policy_store_;

 private:
  MessageLoop loop_;
  BrowserThread ui_thread_;
  BrowserThread file_thread_;
};

TEST_P(ConfigDirPolicyProviderValueTest, Default) {
  ConfigDirPolicyProvider provider(
      ConfigurationPolicyPrefStore::GetChromePolicyDefinitionList(),
      test_dir());
  EXPECT_TRUE(provider.Provide(&policy_store_));
  EXPECT_TRUE(policy_store_.policy_map().empty());
}

TEST_P(ConfigDirPolicyProviderValueTest, NullValue) {
  DictionaryValue dict;
  dict.Set(GetParam().policy_key(), Value::CreateNullValue());
  WriteConfigFile(dict, "empty");
  ConfigDirPolicyProvider provider(
      ConfigurationPolicyPrefStore::GetChromePolicyDefinitionList(),
      test_dir());
  EXPECT_TRUE(provider.Provide(&policy_store_));
  EXPECT_TRUE(policy_store_.policy_map().empty());
}

TEST_P(ConfigDirPolicyProviderValueTest, TestValue) {
  DictionaryValue dict;
  dict.Set(GetParam().policy_key(), GetParam().test_value()->DeepCopy());
  WriteConfigFile(dict, "policy");
  ConfigDirPolicyProvider provider(
      ConfigurationPolicyPrefStore::GetChromePolicyDefinitionList(),
      test_dir());
  EXPECT_TRUE(provider.Provide(&policy_store_));
  EXPECT_EQ(1U, policy_store_.policy_map().size());
  const Value* value = policy_store_.Get(GetParam().type());
  ASSERT_TRUE(value);
  EXPECT_TRUE(GetParam().test_value()->Equals(value));
}

// Test parameters for all supported policies.
INSTANTIATE_TEST_CASE_P(
    ConfigDirPolicyProviderValueTestInstance,
    ConfigDirPolicyProviderValueTest,
    testing::Values(
        ValueTestParams::ForStringPolicy(
            kPolicyHomepageLocation,
            key::kHomepageLocation),
        ValueTestParams::ForBooleanPolicy(
            kPolicyHomepageIsNewTabPage,
            key::kHomepageIsNewTabPage),
        ValueTestParams::ForIntegerPolicy(
            kPolicyRestoreOnStartup,
            key::kRestoreOnStartup),
        ValueTestParams::ForListPolicy(
            kPolicyRestoreOnStartupURLs,
            key::kRestoreOnStartupURLs),
        ValueTestParams::ForBooleanPolicy(
            kPolicyDefaultSearchProviderEnabled,
            key::kDefaultSearchProviderEnabled),
        ValueTestParams::ForStringPolicy(
            kPolicyDefaultSearchProviderName,
            key::kDefaultSearchProviderName),
        ValueTestParams::ForStringPolicy(
            kPolicyDefaultSearchProviderKeyword,
            key::kDefaultSearchProviderKeyword),
        ValueTestParams::ForStringPolicy(
            kPolicyDefaultSearchProviderSearchURL,
            key::kDefaultSearchProviderSearchURL),
        ValueTestParams::ForStringPolicy(
            kPolicyDefaultSearchProviderSuggestURL,
            key::kDefaultSearchProviderSuggestURL),
        ValueTestParams::ForStringPolicy(
            kPolicyDefaultSearchProviderInstantURL,
            key::kDefaultSearchProviderInstantURL),
        ValueTestParams::ForStringPolicy(
            kPolicyDefaultSearchProviderIconURL,
            key::kDefaultSearchProviderIconURL),
        ValueTestParams::ForStringPolicy(
            kPolicyDefaultSearchProviderEncodings,
            key::kDefaultSearchProviderEncodings),
        ValueTestParams::ForStringPolicy(
            kPolicyProxyMode,
            key::kProxyMode),
        ValueTestParams::ForIntegerPolicy(
            kPolicyProxyServerMode,
            key::kProxyServerMode),
        ValueTestParams::ForStringPolicy(
            kPolicyProxyServer,
            key::kProxyServer),
        ValueTestParams::ForStringPolicy(
            kPolicyProxyPacUrl,
            key::kProxyPacUrl),
        ValueTestParams::ForStringPolicy(
            kPolicyProxyBypassList,
            key::kProxyBypassList),
        ValueTestParams::ForBooleanPolicy(
            kPolicyAlternateErrorPagesEnabled,
            key::kAlternateErrorPagesEnabled),
        ValueTestParams::ForBooleanPolicy(
            kPolicySearchSuggestEnabled,
            key::kSearchSuggestEnabled),
        ValueTestParams::ForBooleanPolicy(
            kPolicyDnsPrefetchingEnabled,
            key::kDnsPrefetchingEnabled),
        ValueTestParams::ForBooleanPolicy(
            kPolicySafeBrowsingEnabled,
            key::kSafeBrowsingEnabled),
        ValueTestParams::ForBooleanPolicy(
            kPolicyMetricsReportingEnabled,
            key::kMetricsReportingEnabled),
        ValueTestParams::ForBooleanPolicy(
            kPolicyPasswordManagerEnabled,
            key::kPasswordManagerEnabled),
        ValueTestParams::ForBooleanPolicy(
            kPolicyPasswordManagerAllowShowPasswords,
            key::kPasswordManagerAllowShowPasswords),
        ValueTestParams::ForListPolicy(
            kPolicyDisabledPlugins,
            key::kDisabledPlugins),
        ValueTestParams::ForBooleanPolicy(
            kPolicyAutoFillEnabled,
            key::kAutoFillEnabled),
        ValueTestParams::ForStringPolicy(
            kPolicyApplicationLocaleValue,
            key::kApplicationLocaleValue),
        ValueTestParams::ForBooleanPolicy(
            kPolicySyncDisabled,
            key::kSyncDisabled),
        ValueTestParams::ForListPolicy(
            kPolicyExtensionInstallWhitelist,
            key::kExtensionInstallWhitelist),
        ValueTestParams::ForListPolicy(
            kPolicyExtensionInstallBlacklist,
            key::kExtensionInstallBlacklist),
        ValueTestParams::ForBooleanPolicy(
            kPolicyShowHomeButton,
            key::kShowHomeButton),
        ValueTestParams::ForBooleanPolicy(
            kPolicyPrintingEnabled,
            key::kPrintingEnabled),
        ValueTestParams::ForIntegerPolicy(
            kPolicyPolicyRefreshRate,
            key::kPolicyRefreshRate)));

}  // namespace policy
