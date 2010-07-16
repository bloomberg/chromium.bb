// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "base/command_line.h"
#include "chrome/browser/configuration_policy_pref_store.h"
#include "chrome/browser/mock_configuration_policy_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/chrome_switches.h"

class ConfigurationPolicyPrefStoreTest : public testing::Test {
 public:
  // Applies a policy that has a string value.
  void ApplyStringPolicyValue(
      ConfigurationPolicyPrefStore* store,
      ConfigurationPolicyStore::PolicyType type,
      const wchar_t* policy_value);

  // The following three methods test a policy which controls a string
  // preference.
  // Checks that by default, it's an empty string.
  void TestStringPolicyGetDefault(const wchar_t* pref_name);
  // Checks that values can be set.
  void TestStringPolicySetValue(const wchar_t* pref_name,
                                ConfigurationPolicyStore::PolicyType type);
  // A wrapper that calls the above two methods.
  void TestStringPolicy(const wchar_t* pref_name,
                        ConfigurationPolicyStore::PolicyType type);

  // The following three methods test a policy which controls a boolean
  // preference.
  // Checks that there's no deafult.
  void TestBooleanPolicyGetDefault(const wchar_t* pref_name);
  // Checks that values can be set.
  void TestBooleanPolicySetValue(const wchar_t* pref_name,
                                 ConfigurationPolicyStore::PolicyType type);
  // A wrapper that calls the above two methods.
  void TestBooleanPolicy(const wchar_t* pref_name,
                         ConfigurationPolicyStore::PolicyType type);

  // The following three methods test a policy which controls an integer
  // preference.
  // Checks that by default, it's 0.
  void TestIntegerPolicyGetDefault(const wchar_t* pref_name);
  // Checks that values can be set.
  void TestIntegerPolicySetValue(const wchar_t* pref_name,
                                 ConfigurationPolicyStore::PolicyType type);
  // A wrapper that calls the above two methods.
  void TestIntegerPolicy(const wchar_t* pref_name,
                         ConfigurationPolicyStore::PolicyType type);
};

void ConfigurationPolicyPrefStoreTest::ApplyStringPolicyValue(
    ConfigurationPolicyPrefStore* store,
    ConfigurationPolicyStore::PolicyType type,
    const wchar_t* policy_value) {
  store->Apply(type, Value::CreateStringValue(policy_value));
}

void ConfigurationPolicyPrefStoreTest::TestStringPolicyGetDefault(
    const wchar_t* pref_name) {
  ConfigurationPolicyPrefStore store(NULL, NULL);
  std::wstring result;
  store.prefs()->GetString(pref_name, &result);
  EXPECT_EQ(result, L"");
}

void ConfigurationPolicyPrefStoreTest::TestStringPolicySetValue(
    const wchar_t* pref_name, ConfigurationPolicyStore::PolicyType type) {
  ConfigurationPolicyPrefStore store(NULL, NULL);
  ApplyStringPolicyValue(&store, type, L"http://chromium.org");
  std::wstring result;
  store.prefs()->GetString(pref_name, &result);
  EXPECT_EQ(result, L"http://chromium.org");
}

void ConfigurationPolicyPrefStoreTest::TestStringPolicy(
    const wchar_t* pref_name, ConfigurationPolicyStore::PolicyType type) {
  TestStringPolicyGetDefault(pref_name);
  TestStringPolicySetValue(pref_name, type);
}

void ConfigurationPolicyPrefStoreTest::TestBooleanPolicyGetDefault(
    const wchar_t* pref_name) {
  ConfigurationPolicyPrefStore store(NULL, NULL);
  bool result = false;
  store.prefs()->GetBoolean(pref_name, &result);
  EXPECT_FALSE(result);
  result = true;
  store.prefs()->GetBoolean(pref_name, &result);
  EXPECT_TRUE(result);
}

void ConfigurationPolicyPrefStoreTest::TestBooleanPolicySetValue(
    const wchar_t* pref_name, ConfigurationPolicyStore::PolicyType type) {
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
    const wchar_t* pref_name, ConfigurationPolicyStore::PolicyType type) {
  TestBooleanPolicyGetDefault(pref_name);
  TestBooleanPolicySetValue(pref_name, type);
}

void ConfigurationPolicyPrefStoreTest::TestIntegerPolicyGetDefault(
    const wchar_t* pref_name) {
  ConfigurationPolicyPrefStore store(NULL, NULL);
  int result = 0;
  store.prefs()->GetInteger(pref_name, &result);
  EXPECT_EQ(result, 0);
}

void ConfigurationPolicyPrefStoreTest::TestIntegerPolicySetValue(
    const wchar_t* pref_name, ConfigurationPolicyStore::PolicyType type) {
  ConfigurationPolicyPrefStore store(NULL, NULL);
  store.Apply(type, Value::CreateIntegerValue(2));
  int result = 0;
  store.prefs()->GetInteger(pref_name, &result);
  EXPECT_EQ(result, 2);
}

void ConfigurationPolicyPrefStoreTest::TestIntegerPolicy(
    const wchar_t* pref_name, ConfigurationPolicyStore::PolicyType type) {
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
  FilePath unused_path(FILE_PATH_LITERAL("foo.exe"));
  CommandLine command_line(unused_path);
  command_line.AppendSwitch(switches::kNoProxyServer);
  command_line.AppendSwitch(switches::kProxyAutoDetect);
  command_line.AppendSwitchWithValue(switches::kProxyPacUrl,
                                     L"http://chromium.org/test.pac");
  command_line.AppendSwitchWithValue(switches::kProxyServer,
                                     L"http://chromium2.org");
  command_line.AppendSwitchWithValue(switches::kProxyBypassList,
                                     L"http://chromium3.org");

  ConfigurationPolicyPrefStore store(&command_line, NULL);
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);

  // Ensure that all traces of the command-line specified proxy
  // switches have been overriden.
  std::wstring string_result;
  EXPECT_TRUE(store.prefs()->GetString(prefs::kProxyBypassList,
                                       &string_result));
  EXPECT_EQ(string_result, L"http://chromium3.org");

  EXPECT_TRUE(store.prefs()->GetString(prefs::kProxyPacUrl, &string_result));
  EXPECT_EQ(string_result, L"http://chromium.org/test.pac");
  EXPECT_TRUE(store.prefs()->GetString(prefs::kProxyServer, &string_result));
  EXPECT_EQ(string_result, L"http://chromium2.org");
  bool bool_result;
  EXPECT_TRUE(store.prefs()->GetBoolean(prefs::kNoProxyServer, &bool_result));
  EXPECT_TRUE(bool_result);
  EXPECT_TRUE(store.prefs()->GetBoolean(prefs::kProxyAutoDetect,
                                         &bool_result));
  EXPECT_TRUE(bool_result);
}

TEST_F(ConfigurationPolicyPrefStoreTest, TestPolicyProxyConfigManualOverride) {
  FilePath unused_path(FILE_PATH_LITERAL("foo.exe"));
  CommandLine command_line(unused_path);
  command_line.AppendSwitch(switches::kNoProxyServer);
  command_line.AppendSwitch(switches::kProxyAutoDetect);
  command_line.AppendSwitchWithValue(switches::kProxyPacUrl,
                                     L"http://chromium.org/test.pac");
  command_line.AppendSwitchWithValue(switches::kProxyServer,
                                     L"http://chromium.org");
  command_line.AppendSwitchWithValue(switches::kProxyBypassList,
                                     L"http://chromium.org");

  scoped_ptr<MockConfigurationPolicyProvider> provider(
      new MockConfigurationPolicyProvider());
  provider->AddPolicy(ConfigurationPolicyStore::kPolicyProxyServerMode,
      Value::CreateIntegerValue(
          ConfigurationPolicyStore::kPolicyManuallyConfiguredProxyMode));
  provider->AddPolicy(ConfigurationPolicyStore::kPolicyProxyBypassList,
      Value::CreateStringValue(L"http://chromium.org/override"));

  ConfigurationPolicyPrefStore store(&command_line,
                                     provider.release());
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);

  // Ensure that all traces of the command-line specified proxy
  // switches have been overriden.
  std::wstring string_result;
  EXPECT_TRUE(store.prefs()->GetString(prefs::kProxyBypassList,
                                       &string_result));
  EXPECT_EQ(string_result, L"http://chromium.org/override");

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
  FilePath unused_path(FILE_PATH_LITERAL("foo.exe"));
  CommandLine command_line(unused_path);
  scoped_ptr<MockConfigurationPolicyProvider> provider(
      new MockConfigurationPolicyProvider());
  provider->AddPolicy(ConfigurationPolicyStore::kPolicyProxyBypassList,
      Value::CreateStringValue(L"http://chromium.org/override"));
  provider->AddPolicy(ConfigurationPolicyStore::kPolicyProxyServerMode,
      Value::CreateIntegerValue(
          ConfigurationPolicyStore::kPolicyNoProxyServerMode));

  ConfigurationPolicyPrefStore store(&command_line, provider.release());
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);

  std::wstring string_result;
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
  FilePath unused_path(FILE_PATH_LITERAL("foo.exe"));
  CommandLine command_line(unused_path);
  scoped_ptr<MockConfigurationPolicyProvider> provider(
      new MockConfigurationPolicyProvider());
  provider->AddPolicy(ConfigurationPolicyStore::kPolicyProxyServerMode,
      Value::CreateIntegerValue(
          ConfigurationPolicyStore::kPolicyNoProxyServerMode));
  provider->AddPolicy(ConfigurationPolicyStore::kPolicyProxyBypassList,
      Value::CreateStringValue(L"http://chromium.org/override"));

  ConfigurationPolicyPrefStore store(&command_line, provider.release());
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);

  std::wstring string_result;
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
  FilePath unused_path(FILE_PATH_LITERAL("foo.exe"));
  CommandLine command_line(unused_path);
  scoped_ptr<MockConfigurationPolicyProvider> provider(
      new MockConfigurationPolicyProvider());
  provider->AddPolicy(ConfigurationPolicyStore::kPolicyProxyBypassList,
      Value::CreateStringValue(L"http://chromium.org/override"));
  provider->AddPolicy(ConfigurationPolicyStore::kPolicyProxyServerMode,
      Value::CreateIntegerValue(
          ConfigurationPolicyStore::kPolicyAutoDetectProxyMode));

  ConfigurationPolicyPrefStore store(&command_line, provider.release());
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);

  // Ensure that all traces of the command-line specified proxy
  // switches have been overriden.
  std::wstring string_result;
  EXPECT_TRUE(store.prefs()->GetString(prefs::kProxyBypassList,
                                       &string_result));
  EXPECT_EQ(string_result, L"http://chromium.org/override");

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
  FilePath unused_path(FILE_PATH_LITERAL("foo.exe"));
  CommandLine command_line(unused_path);
  scoped_ptr<MockConfigurationPolicyProvider> provider(
      new MockConfigurationPolicyProvider());
  provider->AddPolicy(ConfigurationPolicyStore::kPolicyProxyBypassList,
      Value::CreateStringValue(L"http://chromium.org/override"));
  provider->AddPolicy(ConfigurationPolicyStore::kPolicyProxyServerMode,
      Value::CreateIntegerValue(
          ConfigurationPolicyStore::kPolicyUseSystemProxyMode));

  ConfigurationPolicyPrefStore store(&command_line, provider.release());
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);

  std::wstring string_result;
  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyBypassList,
                                       &string_result));
  EXPECT_EQ(string_result, L"");

  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyPacUrl, &string_result));
  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyServer, &string_result));
  bool bool_result;
  EXPECT_FALSE(store.prefs()->GetBoolean(prefs::kNoProxyServer, &bool_result));
  EXPECT_FALSE(store.prefs()->GetBoolean(prefs::kProxyAutoDetect,
                                         &bool_result));
}

TEST_F(ConfigurationPolicyPrefStoreTest,
    TestPolicyProxyConfiguseSystemReversedApplyOrder) {
  FilePath unused_path(FILE_PATH_LITERAL("foo.exe"));
  CommandLine command_line(unused_path);
  scoped_ptr<MockConfigurationPolicyProvider> provider(
      new MockConfigurationPolicyProvider());
  provider->AddPolicy(ConfigurationPolicyStore::kPolicyProxyServerMode,
      Value::CreateIntegerValue(
          ConfigurationPolicyStore::kPolicyUseSystemProxyMode));
  provider->AddPolicy(ConfigurationPolicyStore::kPolicyProxyBypassList,
      Value::CreateStringValue(L"http://chromium.org/override"));

  ConfigurationPolicyPrefStore store(&command_line, provider.release());
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);

  std::wstring string_result;
  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyBypassList,
                                       &string_result));
  EXPECT_EQ(string_result, L"");

  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyPacUrl, &string_result));
  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyServer, &string_result));
  bool bool_result;
  EXPECT_FALSE(store.prefs()->GetBoolean(prefs::kNoProxyServer, &bool_result));
  EXPECT_FALSE(store.prefs()->GetBoolean(prefs::kProxyAutoDetect,
                                         &bool_result));
}

TEST_F(ConfigurationPolicyPrefStoreTest,
    TestPolicyProxyDisabledPlugin) {
  FilePath unused_path(FILE_PATH_LITERAL("foo.exe"));
  CommandLine command_line(unused_path);
  scoped_ptr<MockConfigurationPolicyProvider> provider(
      new MockConfigurationPolicyProvider());

  ConfigurationPolicyPrefStore store(&command_line, provider.release());
  ApplyStringPolicyValue(&store,
      ConfigurationPolicyStore::kPolicyDisabledPlugins,
      L"plugin1");
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);

  ListValue* plugin_blacklist = NULL;
  EXPECT_TRUE(store.prefs()->GetList(prefs::kPluginsPluginsBlacklist,
                                     &plugin_blacklist));

  ListValue::const_iterator current(plugin_blacklist->begin());
  ListValue::const_iterator end(plugin_blacklist->end());

  ASSERT_TRUE(current != end);
  std::string plugin_name;
  (*current)->GetAsString(&plugin_name);
  EXPECT_EQ("plugin1", plugin_name);
  ++current;
  EXPECT_TRUE(current == end);
}

TEST_F(ConfigurationPolicyPrefStoreTest,
    TestPolicyProxyDisabledPluginEscapedComma) {
  FilePath unused_path(FILE_PATH_LITERAL("foo.exe"));
  CommandLine command_line(unused_path);
  scoped_ptr<MockConfigurationPolicyProvider> provider(
     new MockConfigurationPolicyProvider());

  ConfigurationPolicyPrefStore store(&command_line, provider.release());
  ApplyStringPolicyValue(&store,
                         ConfigurationPolicyStore::kPolicyDisabledPlugins,
                         L"plugin1,plugin2\\,");
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);

  ListValue* plugin_blacklist = NULL;
  EXPECT_TRUE(store.prefs()->GetList(prefs::kPluginsPluginsBlacklist,
                                     &plugin_blacklist));
  ListValue::const_iterator current(plugin_blacklist->begin());
  ListValue::const_iterator end(plugin_blacklist->end());
  ASSERT_TRUE(current != end);
  std::string plugin_name;
  (*current)->GetAsString(&plugin_name);
  EXPECT_EQ("plugin1", plugin_name);
  ++current;
  ASSERT_TRUE(current != end);
  (*current)->GetAsString(&plugin_name);
  EXPECT_EQ("plugin2,", plugin_name);
  ++current;
  EXPECT_TRUE(current == end);
}

