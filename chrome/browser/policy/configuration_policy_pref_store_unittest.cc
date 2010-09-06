// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "base/command_line.h"
#include "base/file_path.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/chrome_switches.h"

class ConfigurationPolicyPrefStoreTest : public testing::Test {
 public:
  // Applies a policy that has a string value.
  void ApplyStringPolicyValue(
      ConfigurationPolicyPrefStore* store,
      ConfigurationPolicyStore::PolicyType type,
      const char* policy_value);

  // The following three methods test a policy which controls a preference
  // that is a list of strings.
  // Checks that by default, it's an empty list.
  void TestListPolicyGetDefault(const char* pref_name);
  // Checks that values can be set.
  void TestListPolicySetValue(const char* pref_name,
                              ConfigurationPolicyStore::PolicyType type);
  // A wrapper that calls the above two methods.
  void TestListPolicy(const char* pref_name,
                      ConfigurationPolicyStore::PolicyType type);

  // The following three methods test a policy which controls a string
  // preference.
  // Checks that by default, it's an empty string.
  void TestStringPolicyGetDefault(const char* pref_name);
  // Checks that values can be set.
  void TestStringPolicySetValue(const char* pref_name,
                                ConfigurationPolicyStore::PolicyType type);
  // A wrapper that calls the above two methods.
  void TestStringPolicy(const char* pref_name,
                        ConfigurationPolicyStore::PolicyType type);

  // The following three methods test a policy which controls a boolean
  // preference.
  // Checks that there's no deafult.
  void TestBooleanPolicyGetDefault(const char* pref_name);
  // Checks that values can be set.
  void TestBooleanPolicySetValue(const char* pref_name,
                                 ConfigurationPolicyStore::PolicyType type);
  // A wrapper that calls the above two methods.
  void TestBooleanPolicy(const char* pref_name,
                         ConfigurationPolicyStore::PolicyType type);

  // The following three methods test a policy which controls an integer
  // preference.
  // Checks that by default, it's 0.
  void TestIntegerPolicyGetDefault(const char* pref_name);
  // Checks that values can be set.
  void TestIntegerPolicySetValue(const char* pref_name,
                                 ConfigurationPolicyStore::PolicyType type);
  // A wrapper that calls the above two methods.
  void TestIntegerPolicy(const char* pref_name,
                         ConfigurationPolicyStore::PolicyType type);
};

void ConfigurationPolicyPrefStoreTest::ApplyStringPolicyValue(
    ConfigurationPolicyPrefStore* store,
    ConfigurationPolicyStore::PolicyType type,
    const char* policy_value) {
  store->Apply(type, Value::CreateStringValue(policy_value));
}

void ConfigurationPolicyPrefStoreTest::TestListPolicyGetDefault(
    const char* pref_name) {
  ConfigurationPolicyPrefStore store(NULL, NULL);
  ListValue* list = NULL;
  EXPECT_FALSE(store.prefs()->GetList(pref_name, &list));
}

void ConfigurationPolicyPrefStoreTest::TestListPolicySetValue(
    const char* pref_name, ConfigurationPolicyStore::PolicyType type) {
  ConfigurationPolicyPrefStore store(NULL, NULL);
  ListValue* in_value = new ListValue();
  in_value->Append(Value::CreateStringValue("test1"));
  in_value->Append(Value::CreateStringValue("test2,"));
  store.Apply(type, in_value);
  ListValue* list = NULL;
  EXPECT_TRUE(store.prefs()->GetList(pref_name, &list));
  ListValue::const_iterator current(list->begin());
  ListValue::const_iterator end(list->end());
  ASSERT_TRUE(current != end);
  std::string value;
  (*current)->GetAsString(&value);
  EXPECT_EQ("test1", value);
  ++current;
  ASSERT_TRUE(current != end);
  (*current)->GetAsString(&value);
  EXPECT_EQ("test2,", value);
  ++current;
  EXPECT_TRUE(current == end);
}

void ConfigurationPolicyPrefStoreTest::TestListPolicy(
    const char* pref_name, ConfigurationPolicyStore::PolicyType type) {
  TestListPolicyGetDefault(pref_name);
  TestListPolicySetValue(pref_name, type);
}

void ConfigurationPolicyPrefStoreTest::TestStringPolicyGetDefault(
    const char* pref_name) {
  ConfigurationPolicyPrefStore store(NULL, NULL);
  std::string result;
  store.prefs()->GetString(pref_name, &result);
  EXPECT_EQ(result, "");
}

void ConfigurationPolicyPrefStoreTest::TestStringPolicySetValue(
    const char* pref_name, ConfigurationPolicyStore::PolicyType type) {
  ConfigurationPolicyPrefStore store(NULL, NULL);
  ApplyStringPolicyValue(&store, type, "http://chromium.org");
  std::string result;
  store.prefs()->GetString(pref_name, &result);
  EXPECT_EQ(result, "http://chromium.org");
}

void ConfigurationPolicyPrefStoreTest::TestStringPolicy(
    const char* pref_name, ConfigurationPolicyStore::PolicyType type) {
  TestStringPolicyGetDefault(pref_name);
  TestStringPolicySetValue(pref_name, type);
}

void ConfigurationPolicyPrefStoreTest::TestBooleanPolicyGetDefault(
    const char* pref_name) {
  ConfigurationPolicyPrefStore store(NULL, NULL);
  bool result = false;
  store.prefs()->GetBoolean(pref_name, &result);
  EXPECT_FALSE(result);
  result = true;
  store.prefs()->GetBoolean(pref_name, &result);
  EXPECT_TRUE(result);
}

void ConfigurationPolicyPrefStoreTest::TestBooleanPolicySetValue(
    const char* pref_name, ConfigurationPolicyStore::PolicyType type) {
  ConfigurationPolicyPrefStore store(NULL, NULL);
  store.Apply(type, Value::CreateBooleanValue(false));
  bool result = true;
  store.prefs()->GetBoolean(pref_name, &result);
  EXPECT_FALSE(result);

  store.Apply(type, Value::CreateBooleanValue(true));
  result = false;
  store.prefs()->GetBoolean(pref_name, &result);
  EXPECT_TRUE(result);
}

void ConfigurationPolicyPrefStoreTest::TestBooleanPolicy(
    const char* pref_name, ConfigurationPolicyStore::PolicyType type) {
  TestBooleanPolicyGetDefault(pref_name);
  TestBooleanPolicySetValue(pref_name, type);
}

void ConfigurationPolicyPrefStoreTest::TestIntegerPolicyGetDefault(
    const char* pref_name) {
  ConfigurationPolicyPrefStore store(NULL, NULL);
  int result = 0;
  store.prefs()->GetInteger(pref_name, &result);
  EXPECT_EQ(result, 0);
}

void ConfigurationPolicyPrefStoreTest::TestIntegerPolicySetValue(
    const char* pref_name, ConfigurationPolicyStore::PolicyType type) {
  ConfigurationPolicyPrefStore store(NULL, NULL);
  store.Apply(type, Value::CreateIntegerValue(2));
  int result = 0;
  store.prefs()->GetInteger(pref_name, &result);
  EXPECT_EQ(result, 2);
}

void ConfigurationPolicyPrefStoreTest::TestIntegerPolicy(
    const char* pref_name, ConfigurationPolicyStore::PolicyType type) {
  TestIntegerPolicyGetDefault(pref_name);
  TestIntegerPolicySetValue(pref_name, type);
}

TEST_F(ConfigurationPolicyPrefStoreTest, TestSettingHomePageDefault) {
  TestStringPolicy(prefs::kHomePage,
                   ConfigurationPolicyPrefStore::kPolicyHomePage);
}

TEST_F(ConfigurationPolicyPrefStoreTest, TestPolicyHomepageIsNewTabPage) {
  TestBooleanPolicy(prefs::kHomePageIsNewTabPage,
      ConfigurationPolicyPrefStore::kPolicyHomepageIsNewTabPage);
}

TEST_F(ConfigurationPolicyPrefStoreTest, TestPolicyRestoreOnStartup) {
  TestIntegerPolicy(prefs::kRestoreOnStartup,
      ConfigurationPolicyPrefStore::kPolicyRestoreOnStartup);
  TestListPolicy(prefs::kURLsToRestoreOnStartup,
      ConfigurationPolicyPrefStore::kPolicyURLsToRestoreOnStartup);
}

TEST_F(ConfigurationPolicyPrefStoreTest, TestPolicyDefaultSearchProvider) {
  TestStringPolicy(prefs::kDefaultSearchProviderName,
      ConfigurationPolicyPrefStore::kPolicyDefaultSearchProviderName);
  TestStringPolicy(prefs::kDefaultSearchProviderKeyword,
      ConfigurationPolicyPrefStore::kPolicyDefaultSearchProviderKeyword);
  TestStringPolicy(prefs::kDefaultSearchProviderSearchURL,
      ConfigurationPolicyPrefStore::kPolicyDefaultSearchProviderSearchURL);
  TestStringPolicy(prefs::kDefaultSearchProviderSuggestURL,
      ConfigurationPolicyPrefStore::kPolicyDefaultSearchProviderSuggestURL);
  TestStringPolicy(prefs::kDefaultSearchProviderIconURL,
      ConfigurationPolicyPrefStore::kPolicyDefaultSearchProviderIconURL);
  TestStringPolicy(prefs::kDefaultSearchProviderEncodings,
      ConfigurationPolicyPrefStore::kPolicyDefaultSearchProviderEncodings);
}

TEST_F(ConfigurationPolicyPrefStoreTest, TestPolicyAlternateErrorPagesEnabled) {
  TestBooleanPolicy(prefs::kAlternateErrorPagesEnabled,
      ConfigurationPolicyStore::kPolicyAlternateErrorPagesEnabled);
}

TEST_F(ConfigurationPolicyPrefStoreTest, TestPolicySearchSuggestEnabled) {
  TestBooleanPolicy(prefs::kSearchSuggestEnabled,
      ConfigurationPolicyStore::kPolicySearchSuggestEnabled);
}

TEST_F(ConfigurationPolicyPrefStoreTest, TestPolicyDnsPrefetchingEnabled) {
  TestBooleanPolicy(prefs::kDnsPrefetchingEnabled,
      ConfigurationPolicyStore::kPolicyDnsPrefetchingEnabled);
}

TEST_F(ConfigurationPolicyPrefStoreTest, TestPolicySafeBrowsingEnabled) {
  TestBooleanPolicy(prefs::kSafeBrowsingEnabled,
      ConfigurationPolicyStore::kPolicySafeBrowsingEnabled);
}

TEST_F(ConfigurationPolicyPrefStoreTest, TestPolicyMetricsReportingEnabled) {
  TestBooleanPolicy(prefs::kMetricsReportingEnabled,
      ConfigurationPolicyStore::kPolicyMetricsReportingEnabled);
}

TEST_F(ConfigurationPolicyPrefStoreTest, TestPolicyPasswordManagerEnabled) {
  TestBooleanPolicy(prefs::kPasswordManagerEnabled,
      ConfigurationPolicyStore::kPolicyPasswordManagerEnabled);
}

TEST_F(ConfigurationPolicyPrefStoreTest, TestSettingProxyServer) {
  TestStringPolicy(prefs::kProxyServer,
                   ConfigurationPolicyPrefStore::kPolicyProxyServer);
}

TEST_F(ConfigurationPolicyPrefStoreTest, TestSettingProxyPacUrl) {
  TestStringPolicy(prefs::kProxyPacUrl,
                   ConfigurationPolicyPrefStore::kPolicyProxyPacUrl);
}

TEST_F(ConfigurationPolicyPrefStoreTest, TestSettingProxyBypassList) {
  TestStringPolicy(prefs::kProxyBypassList,
                   ConfigurationPolicyPrefStore::kPolicyProxyBypassList);
}

TEST_F(ConfigurationPolicyPrefStoreTest, TestSettingsProxyConfig) {
  CommandLine command_line(CommandLine::ARGUMENTS_ONLY);
  command_line.AppendSwitch(switches::kNoProxyServer);
  command_line.AppendSwitch(switches::kProxyAutoDetect);
  command_line.AppendSwitchASCII(switches::kProxyPacUrl,
                                 "http://chromium.org/test.pac");
  command_line.AppendSwitchASCII(switches::kProxyServer,
                                 "http://chromium2.org");
  command_line.AppendSwitchASCII(switches::kProxyBypassList,
                                 "http://chromium3.org");

  ConfigurationPolicyPrefStore store(&command_line, NULL);
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);

  // Ensure that all traces of the command-line specified proxy
  // switches have been overriden.
  std::string string_result;
  EXPECT_TRUE(store.prefs()->GetString(prefs::kProxyBypassList,
                                       &string_result));
  EXPECT_EQ(string_result, "http://chromium3.org");

  EXPECT_TRUE(store.prefs()->GetString(prefs::kProxyPacUrl, &string_result));
  EXPECT_EQ(string_result, "http://chromium.org/test.pac");
  EXPECT_TRUE(store.prefs()->GetString(prefs::kProxyServer, &string_result));
  EXPECT_EQ(string_result, "http://chromium2.org");
  bool bool_result;
  EXPECT_TRUE(store.prefs()->GetBoolean(prefs::kNoProxyServer, &bool_result));
  EXPECT_TRUE(bool_result);
  EXPECT_TRUE(store.prefs()->GetBoolean(prefs::kProxyAutoDetect,
                                         &bool_result));
  EXPECT_TRUE(bool_result);
}

TEST_F(ConfigurationPolicyPrefStoreTest, TestPolicyProxyConfigManualOverride) {
  CommandLine command_line(CommandLine::ARGUMENTS_ONLY);
  command_line.AppendSwitch(switches::kNoProxyServer);
  command_line.AppendSwitch(switches::kProxyAutoDetect);
  command_line.AppendSwitchASCII(switches::kProxyPacUrl,
                                 "http://chromium.org/test.pac");
  command_line.AppendSwitchASCII(switches::kProxyServer,
                                 "http://chromium.org");
  command_line.AppendSwitchASCII(switches::kProxyBypassList,
                                 "http://chromium.org");

  scoped_ptr<MockConfigurationPolicyProvider> provider(
      new MockConfigurationPolicyProvider());
  provider->AddPolicy(ConfigurationPolicyStore::kPolicyProxyServerMode,
      Value::CreateIntegerValue(
          ConfigurationPolicyStore::kPolicyManuallyConfiguredProxyMode));
  provider->AddPolicy(ConfigurationPolicyStore::kPolicyProxyBypassList,
      Value::CreateStringValue("http://chromium.org/override"));

  ConfigurationPolicyPrefStore store(&command_line, provider.get());
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);

  // Ensure that all traces of the command-line specified proxy
  // switches have been overriden.
  std::string string_result;
  EXPECT_TRUE(store.prefs()->GetString(prefs::kProxyBypassList,
                                       &string_result));
  EXPECT_EQ(string_result, "http://chromium.org/override");

  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyPacUrl, &string_result));
  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyServer, &string_result));
  bool bool_result;
  EXPECT_TRUE(store.prefs()->GetBoolean(prefs::kNoProxyServer, &bool_result));
  EXPECT_FALSE(bool_result);
  EXPECT_TRUE(store.prefs()->GetBoolean(prefs::kProxyAutoDetect,
                                        &bool_result));
  EXPECT_FALSE(bool_result);
}

TEST_F(ConfigurationPolicyPrefStoreTest, TestPolicyProxyConfigNoProxy) {
  CommandLine command_line(CommandLine::ARGUMENTS_ONLY);
  scoped_ptr<MockConfigurationPolicyProvider> provider(
      new MockConfigurationPolicyProvider());
  provider->AddPolicy(ConfigurationPolicyStore::kPolicyProxyBypassList,
      Value::CreateStringValue("http://chromium.org/override"));
  provider->AddPolicy(ConfigurationPolicyStore::kPolicyProxyServerMode,
      Value::CreateIntegerValue(
          ConfigurationPolicyStore::kPolicyNoProxyServerMode));

  ConfigurationPolicyPrefStore store(&command_line, provider.get());
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);

  std::string string_result;
  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyBypassList,
                                        &string_result));

  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyPacUrl, &string_result));
  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyServer, &string_result));
  bool bool_result;
  EXPECT_TRUE(store.prefs()->GetBoolean(prefs::kNoProxyServer, &bool_result));
  EXPECT_TRUE(bool_result);
  EXPECT_TRUE(store.prefs()->GetBoolean(prefs::kProxyAutoDetect,
                                        &bool_result));
  EXPECT_FALSE(bool_result);
}

TEST_F(ConfigurationPolicyPrefStoreTest,
    TestPolicyProxyConfigNoProxyReversedApplyOrder) {
  CommandLine command_line(CommandLine::ARGUMENTS_ONLY);
  scoped_ptr<MockConfigurationPolicyProvider> provider(
      new MockConfigurationPolicyProvider());
  provider->AddPolicy(ConfigurationPolicyStore::kPolicyProxyServerMode,
      Value::CreateIntegerValue(
          ConfigurationPolicyStore::kPolicyNoProxyServerMode));
  provider->AddPolicy(ConfigurationPolicyStore::kPolicyProxyBypassList,
      Value::CreateStringValue("http://chromium.org/override"));

  ConfigurationPolicyPrefStore store(&command_line, provider.get());
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);

  std::string string_result;
  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyBypassList,
                                        &string_result));

  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyPacUrl, &string_result));
  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyServer, &string_result));
  bool bool_result;
  EXPECT_TRUE(store.prefs()->GetBoolean(prefs::kNoProxyServer, &bool_result));
  EXPECT_TRUE(bool_result);
  EXPECT_TRUE(store.prefs()->GetBoolean(prefs::kProxyAutoDetect,
                                        &bool_result));
  EXPECT_FALSE(bool_result);
}

TEST_F(ConfigurationPolicyPrefStoreTest, TestPolicyProxyConfigAutoDetect) {
  CommandLine command_line(CommandLine::ARGUMENTS_ONLY);
  scoped_ptr<MockConfigurationPolicyProvider> provider(
      new MockConfigurationPolicyProvider());
  provider->AddPolicy(ConfigurationPolicyStore::kPolicyProxyBypassList,
      Value::CreateStringValue("http://chromium.org/override"));
  provider->AddPolicy(ConfigurationPolicyStore::kPolicyProxyServerMode,
      Value::CreateIntegerValue(
          ConfigurationPolicyStore::kPolicyAutoDetectProxyMode));

  ConfigurationPolicyPrefStore store(&command_line, provider.get());
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);

  // Ensure that all traces of the command-line specified proxy
  // switches have been overriden.
  std::string string_result;
  EXPECT_TRUE(store.prefs()->GetString(prefs::kProxyBypassList,
                                       &string_result));
  EXPECT_EQ(string_result, "http://chromium.org/override");

  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyPacUrl, &string_result));
  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyServer, &string_result));
  bool bool_result;
  EXPECT_TRUE(store.prefs()->GetBoolean(prefs::kNoProxyServer, &bool_result));
  EXPECT_FALSE(bool_result);
  EXPECT_TRUE(store.prefs()->GetBoolean(prefs::kProxyAutoDetect,
                                        &bool_result));
  EXPECT_TRUE(bool_result);
}

TEST_F(ConfigurationPolicyPrefStoreTest, TestPolicyProxyConfiguseSystem) {
  CommandLine command_line(CommandLine::ARGUMENTS_ONLY);
  scoped_ptr<MockConfigurationPolicyProvider> provider(
      new MockConfigurationPolicyProvider());
  provider->AddPolicy(ConfigurationPolicyStore::kPolicyProxyBypassList,
      Value::CreateStringValue("http://chromium.org/override"));
  provider->AddPolicy(ConfigurationPolicyStore::kPolicyProxyServerMode,
      Value::CreateIntegerValue(
          ConfigurationPolicyStore::kPolicyUseSystemProxyMode));

  ConfigurationPolicyPrefStore store(&command_line, provider.get());
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);

  std::string string_result;
  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyBypassList,
                                       &string_result));
  EXPECT_EQ(string_result, "");

  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyPacUrl, &string_result));
  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyServer, &string_result));
  bool bool_result;
  EXPECT_FALSE(store.prefs()->GetBoolean(prefs::kNoProxyServer, &bool_result));
  EXPECT_FALSE(store.prefs()->GetBoolean(prefs::kProxyAutoDetect,
                                         &bool_result));
}

TEST_F(ConfigurationPolicyPrefStoreTest,
    TestPolicyProxyConfiguseSystemReversedApplyOrder) {
  CommandLine command_line(CommandLine::ARGUMENTS_ONLY);
  scoped_ptr<MockConfigurationPolicyProvider> provider(
      new MockConfigurationPolicyProvider());
  provider->AddPolicy(ConfigurationPolicyStore::kPolicyProxyServerMode,
      Value::CreateIntegerValue(
          ConfigurationPolicyStore::kPolicyUseSystemProxyMode));
  provider->AddPolicy(ConfigurationPolicyStore::kPolicyProxyBypassList,
      Value::CreateStringValue("http://chromium.org/override"));

  ConfigurationPolicyPrefStore store(&command_line, provider.get());
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);

  std::string string_result;
  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyBypassList,
                                       &string_result));
  EXPECT_EQ(string_result, "");

  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyPacUrl, &string_result));
  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyServer, &string_result));
  bool bool_result;
  EXPECT_FALSE(store.prefs()->GetBoolean(prefs::kNoProxyServer, &bool_result));
  EXPECT_FALSE(store.prefs()->GetBoolean(prefs::kProxyAutoDetect,
                                         &bool_result));
}

// Checks that if the policy for default search is valid, i.e. there's a
// search URL, that all the elements have been given proper defaults.
TEST_F(ConfigurationPolicyPrefStoreTest, MinimallyDefinedDefaultSearchPolicy) {
  const char* search_url = "http://test.com/search?t={searchTerms}";
  scoped_ptr<MockConfigurationPolicyProvider> provider(
      new MockConfigurationPolicyProvider());
  provider->AddPolicy(
      ConfigurationPolicyStore::kPolicyDefaultSearchProviderSearchURL,
      Value::CreateStringValue(search_url));

  CommandLine command_line(CommandLine::ARGUMENTS_ONLY);
  ConfigurationPolicyPrefStore store(&command_line, provider.get());

  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);
  DictionaryValue* prefs = store.prefs();

  std::string string_result;
  EXPECT_TRUE(prefs->GetString(prefs::kDefaultSearchProviderSearchURL,
                               &string_result));
  EXPECT_EQ(string_result, search_url);

  EXPECT_TRUE(prefs->GetString(prefs::kDefaultSearchProviderName,
                               &string_result));
  EXPECT_EQ(string_result, "test.com");

  EXPECT_TRUE(prefs->GetString(prefs::kDefaultSearchProviderKeyword,
                               &string_result));
  EXPECT_EQ(string_result, "");

  EXPECT_TRUE(prefs->GetString(prefs::kDefaultSearchProviderSuggestURL,
                               &string_result));
  EXPECT_EQ(string_result, "");

  EXPECT_TRUE(prefs->GetString(prefs::kDefaultSearchProviderIconURL,
                               &string_result));
  EXPECT_EQ(string_result, "");

  EXPECT_TRUE(prefs->GetString(prefs::kDefaultSearchProviderEncodings,
                               &string_result));
  EXPECT_EQ(string_result, "");
}

// Checks that for a fully defined search policy, all elements have been
// read properly.
TEST_F(ConfigurationPolicyPrefStoreTest, FullyDefinedDefaultSearchPolicy) {
  const char* search_url = "http://test.com/search?t={searchTerms}";
  const char* suggest_url = "http://test.com/sugg?={searchTerms}";
  const char* icon_url = "http://test.com/icon.jpg";
  const char* name = "MyName";
  const char* keyword = "MyKeyword";
  const char* encodings = "UTF-16;UTF-8";
  scoped_ptr<MockConfigurationPolicyProvider> provider(
      new MockConfigurationPolicyProvider());
  provider->AddPolicy(
      ConfigurationPolicyStore::kPolicyDefaultSearchProviderSearchURL,
      Value::CreateStringValue(search_url));
  provider->AddPolicy(
      ConfigurationPolicyStore::kPolicyDefaultSearchProviderSearchURL,
      Value::CreateStringValue(search_url));
  provider->AddPolicy(
      ConfigurationPolicyStore::kPolicyDefaultSearchProviderName,
      Value::CreateStringValue(name));
  provider->AddPolicy(
      ConfigurationPolicyStore::kPolicyDefaultSearchProviderKeyword,
      Value::CreateStringValue(keyword));
  provider->AddPolicy(
      ConfigurationPolicyStore::kPolicyDefaultSearchProviderSuggestURL,
      Value::CreateStringValue(suggest_url));
  provider->AddPolicy(
      ConfigurationPolicyStore::kPolicyDefaultSearchProviderIconURL,
      Value::CreateStringValue(icon_url));
  provider->AddPolicy(
      ConfigurationPolicyStore::kPolicyDefaultSearchProviderEncodings,
      Value::CreateStringValue(encodings));

  CommandLine command_line(CommandLine::ARGUMENTS_ONLY);
  ConfigurationPolicyPrefStore store(&command_line, provider.get());
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);
  DictionaryValue* prefs = store.prefs();

  std::string result_search_url;
  EXPECT_TRUE(prefs->GetString(prefs::kDefaultSearchProviderSearchURL,
                               &result_search_url));
  EXPECT_EQ(result_search_url, search_url);

  std::string result_name;
  EXPECT_TRUE(prefs->GetString(prefs::kDefaultSearchProviderName,
                               &result_name));
  EXPECT_EQ(result_name, name);

  std::string result_keyword;
  EXPECT_TRUE(prefs->GetString(prefs::kDefaultSearchProviderKeyword,
                               &result_keyword));
  EXPECT_EQ(result_keyword, keyword);

  std::string result_suggest_url;
  EXPECT_TRUE(prefs->GetString(prefs::kDefaultSearchProviderSuggestURL,
                               &result_suggest_url));
  EXPECT_EQ(result_suggest_url, suggest_url);

  std::string result_icon_url;
  EXPECT_TRUE(prefs->GetString(prefs::kDefaultSearchProviderIconURL,
                               &result_icon_url));
  EXPECT_EQ(result_icon_url, icon_url);

  std::string result_encodings;
  EXPECT_TRUE(prefs->GetString(prefs::kDefaultSearchProviderEncodings,
                               &result_encodings));
  EXPECT_EQ(result_encodings, encodings);
}

// Checks that if the default search policy is missing, that no elements of the
// default search policy will be present.
TEST_F(ConfigurationPolicyPrefStoreTest, MissingUrlDefaultSearchPolicy) {
  const char* suggest_url = "http://test.com/sugg?t={searchTerms}";
  const char* icon_url = "http://test.com/icon.jpg";
  const char* name = "MyName";
  const char* keyword = "MyKeyword";
  const char* encodings = "UTF-16;UTF-8";
  scoped_ptr<MockConfigurationPolicyProvider> provider(
      new MockConfigurationPolicyProvider());
  provider->AddPolicy(
      ConfigurationPolicyStore::kPolicyDefaultSearchProviderName,
      Value::CreateStringValue(name));
  provider->AddPolicy(
      ConfigurationPolicyStore::kPolicyDefaultSearchProviderKeyword,
      Value::CreateStringValue(keyword));
  provider->AddPolicy(
      ConfigurationPolicyStore::kPolicyDefaultSearchProviderSuggestURL,
      Value::CreateStringValue(suggest_url));
  provider->AddPolicy(
      ConfigurationPolicyStore::kPolicyDefaultSearchProviderIconURL,
      Value::CreateStringValue(icon_url));
  provider->AddPolicy(
      ConfigurationPolicyStore::kPolicyDefaultSearchProviderEncodings,
      Value::CreateStringValue(encodings));

  CommandLine command_line(CommandLine::ARGUMENTS_ONLY);
  ConfigurationPolicyPrefStore store(&command_line, provider.get());
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);
  DictionaryValue* prefs = store.prefs();

  std::string string_result;
  EXPECT_FALSE(prefs->GetString(prefs::kDefaultSearchProviderSearchURL,
                               &string_result));
  EXPECT_FALSE(prefs->GetString(prefs::kDefaultSearchProviderName,
                               &string_result));
  EXPECT_FALSE(prefs->GetString(prefs::kDefaultSearchProviderKeyword,
                               &string_result));
  EXPECT_FALSE(prefs->GetString(prefs::kDefaultSearchProviderSuggestURL,
                               &string_result));
  EXPECT_FALSE(prefs->GetString(prefs::kDefaultSearchProviderIconURL,
                               &string_result));
  EXPECT_FALSE(prefs->GetString(prefs::kDefaultSearchProviderEncodings,
                               &string_result));
}

// Checks that if the default search policy is invalid, that no elements of the
// default search policy will be present.
TEST_F(ConfigurationPolicyPrefStoreTest, InvalidDefaultSearchPolicy) {
  const char* bad_search_url = "http://test.com/noSearchTerms";
  const char* suggest_url = "http://test.com/sugg?t={searchTerms}";
  const char* icon_url = "http://test.com/icon.jpg";
  const char* name = "MyName";
  const char* keyword = "MyKeyword";
  const char* encodings = "UTF-16;UTF-8";
  scoped_ptr<MockConfigurationPolicyProvider> provider(
      new MockConfigurationPolicyProvider());
  provider->AddPolicy(
      ConfigurationPolicyStore::kPolicyDefaultSearchProviderSearchURL,
      Value::CreateStringValue(bad_search_url));
  provider->AddPolicy(
      ConfigurationPolicyStore::kPolicyDefaultSearchProviderName,
      Value::CreateStringValue(name));
  provider->AddPolicy(
      ConfigurationPolicyStore::kPolicyDefaultSearchProviderKeyword,
      Value::CreateStringValue(keyword));
  provider->AddPolicy(
      ConfigurationPolicyStore::kPolicyDefaultSearchProviderSuggestURL,
      Value::CreateStringValue(suggest_url));
  provider->AddPolicy(
      ConfigurationPolicyStore::kPolicyDefaultSearchProviderIconURL,
      Value::CreateStringValue(icon_url));
  provider->AddPolicy(
      ConfigurationPolicyStore::kPolicyDefaultSearchProviderEncodings,
      Value::CreateStringValue(encodings));

  CommandLine command_line(CommandLine::ARGUMENTS_ONLY);
  ConfigurationPolicyPrefStore store(&command_line, provider.get());
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);
  DictionaryValue* prefs = store.prefs();

  std::string string_result;
  EXPECT_FALSE(prefs->GetString(prefs::kDefaultSearchProviderSearchURL,
                               &string_result));
  EXPECT_FALSE(prefs->GetString(prefs::kDefaultSearchProviderName,
                               &string_result));
  EXPECT_FALSE(prefs->GetString(prefs::kDefaultSearchProviderKeyword,
                               &string_result));
  EXPECT_FALSE(prefs->GetString(prefs::kDefaultSearchProviderSuggestURL,
                               &string_result));
  EXPECT_FALSE(prefs->GetString(prefs::kDefaultSearchProviderIconURL,
                               &string_result));
  EXPECT_FALSE(prefs->GetString(prefs::kDefaultSearchProviderEncodings,
                               &string_result));
}

