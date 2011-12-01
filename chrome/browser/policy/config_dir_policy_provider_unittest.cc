// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/json/json_value_serializer.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/policy/config_dir_policy_provider.h"
#include "chrome/browser/policy/configuration_policy_provider_test.h"

namespace policy {

namespace {

class TestHarness : public PolicyProviderTestHarness {
 public:
  TestHarness();
  virtual ~TestHarness();

  virtual void SetUp() OVERRIDE;

  virtual AsynchronousPolicyProvider* CreateProvider(
      const PolicyDefinitionList* policy_definition_list) OVERRIDE;

  virtual void InstallEmptyPolicy() OVERRIDE;
  virtual void InstallStringPolicy(const std::string& policy_name,
                                   const std::string& policy_value) OVERRIDE;
  virtual void InstallIntegerPolicy(const std::string& policy_name,
                                    int policy_value) OVERRIDE;
  virtual void InstallBooleanPolicy(const std::string& policy_name,
                                    bool policy_value) OVERRIDE;
  virtual void InstallStringListPolicy(const std::string& policy_name,
                                       const ListValue* policy_value) OVERRIDE;

  const FilePath& test_dir() { return test_dir_.path(); }

  // JSON-encode a dictionary and write it to a file.
  void WriteConfigFile(const base::DictionaryValue& dict,
                       const std::string& file_name);

  static PolicyProviderTestHarness* Create();

 private:
  ScopedTempDir test_dir_;

  DISALLOW_COPY_AND_ASSIGN(TestHarness);
};

TestHarness::TestHarness() {}

TestHarness::~TestHarness() {}

void TestHarness::SetUp() {
  ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
}

AsynchronousPolicyProvider* TestHarness::CreateProvider(
    const PolicyDefinitionList* policy_definition_list) {
  return new ConfigDirPolicyProvider(policy_definition_list, test_dir());
}

void TestHarness::InstallEmptyPolicy() {
  DictionaryValue dict;
  WriteConfigFile(dict, "policy");
}

void TestHarness::InstallStringPolicy(const std::string& policy_name,
                                      const std::string& policy_value) {
  DictionaryValue dict;
  dict.SetString(policy_name, policy_value);
  WriteConfigFile(dict, "policy");
}

void TestHarness::InstallIntegerPolicy(const std::string& policy_name,
                                       int policy_value) {
  DictionaryValue dict;
  dict.SetInteger(policy_name, policy_value);
  WriteConfigFile(dict, "policy");
}

void TestHarness::InstallBooleanPolicy(const std::string& policy_name,
                                       bool policy_value) {
  DictionaryValue dict;
  dict.SetBoolean(policy_name, policy_value);
  WriteConfigFile(dict, "policy");
}

void TestHarness::InstallStringListPolicy(const std::string& policy_name,
                                          const ListValue* policy_value) {
  DictionaryValue dict;
  dict.Set(policy_name, policy_value->DeepCopy());
  WriteConfigFile(dict, "policy");
}

void TestHarness::WriteConfigFile(const base::DictionaryValue& dict,
                                  const std::string& file_name) {
  std::string data;
  JSONStringValueSerializer serializer(&data);
  serializer.Serialize(dict);
  const FilePath file_path(test_dir().AppendASCII(file_name));
  ASSERT_TRUE(file_util::WriteFile(file_path, data.c_str(), data.size()));
}

// static
PolicyProviderTestHarness* TestHarness::Create() {
  return new TestHarness();
}

}  // namespace

// Instantiate abstract test case for basic policy reading tests.
INSTANTIATE_TEST_CASE_P(
    ConfigDirPolicyProviderTest,
    ConfigurationPolicyProviderTest,
    testing::Values(TestHarness::Create));

// Some tests that exercise special functionality in ConfigDirPolicyLoader.
class ConfigDirPolicyLoaderTest : public testing::Test {
 protected:
  void SetUp() {
    harness_.SetUp();
  }

  TestHarness harness_;
};

// The preferences dictionary is expected to be empty when there are no files to
// load.
TEST_F(ConfigDirPolicyLoaderTest, ReadPrefsEmpty) {
  ConfigDirPolicyProviderDelegate loader(harness_.test_dir());
  scoped_ptr<base::DictionaryValue> policy(loader.Load());
  EXPECT_TRUE(policy.get());
  EXPECT_TRUE(policy->empty());
}

// Reading from a non-existent directory should result in an empty preferences
// dictionary.
TEST_F(ConfigDirPolicyLoaderTest, ReadPrefsNonExistentDirectory) {
  FilePath non_existent_dir(
      harness_.test_dir().Append(FILE_PATH_LITERAL("not_there")));
  ConfigDirPolicyProviderDelegate loader(non_existent_dir);
  scoped_ptr<base::DictionaryValue> policy(loader.Load());
  EXPECT_TRUE(policy.get());
  EXPECT_TRUE(policy->empty());
}

// Test merging values from different files.
TEST_F(ConfigDirPolicyLoaderTest, ReadPrefsMergePrefs) {
  // Write a bunch of data files in order to increase the chance to detect the
  // provider not respecting lexicographic ordering when reading them. Since the
  // filesystem may return files in arbitrary order, there is no way to be sure,
  // but this is better than nothing.
  base::DictionaryValue test_dict_bar;
  test_dict_bar.SetString("HomepageLocation", "http://bar.com");
  for (unsigned int i = 1; i <= 4; ++i)
    harness_.WriteConfigFile(test_dict_bar, base::IntToString(i));
  base::DictionaryValue test_dict_foo;
  test_dict_foo.SetString("HomepageLocation", "http://foo.com");
  harness_.WriteConfigFile(test_dict_foo, "9");
  for (unsigned int i = 5; i <= 8; ++i)
    harness_.WriteConfigFile(test_dict_bar, base::IntToString(i));

  ConfigDirPolicyProviderDelegate loader(harness_.test_dir());
  scoped_ptr<base::DictionaryValue> policy(loader.Load());
  EXPECT_TRUE(policy.get());
  EXPECT_TRUE(policy->Equals(&test_dict_foo));
}

}  // namespace policy
