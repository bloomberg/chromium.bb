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

namespace policy {

// Holds a set of test parameters, consisting of pref name and policy type.
class TypeAndName {
 public:
  TypeAndName(ConfigurationPolicyStore::PolicyType type, const char* pref_name)
      : type_(type),
        pref_name_(pref_name) {}

  ConfigurationPolicyStore::PolicyType type() const { return type_; }
  const char* pref_name() const { return pref_name_; }

 private:
  ConfigurationPolicyStore::PolicyType type_;
  const char* pref_name_;
};

// Test cases for list-valued policy settings.
class ConfigurationPolicyPrefStoreListTest
    : public testing::TestWithParam<TypeAndName> {
};

TEST_P(ConfigurationPolicyPrefStoreListTest, GetDefault) {
  ConfigurationPolicyPrefStore store(NULL, NULL);
  ListValue* list = NULL;
  EXPECT_FALSE(store.prefs()->GetList(GetParam().pref_name(), &list));
}

TEST_P(ConfigurationPolicyPrefStoreListTest, SetValue) {
  ConfigurationPolicyPrefStore store(NULL, NULL);
  ListValue* in_value = new ListValue();
  in_value->Append(Value::CreateStringValue("test1"));
  in_value->Append(Value::CreateStringValue("test2,"));
  store.Apply(GetParam().type(), in_value);
  ListValue* list = NULL;
  EXPECT_TRUE(store.prefs()->GetList(GetParam().pref_name(), &list));
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

INSTANTIATE_TEST_CASE_P(
    ConfigurationPolicyPrefStoreListTestInstance,
    ConfigurationPolicyPrefStoreListTest,
    testing::Values(
        TypeAndName(ConfigurationPolicyStore::kPolicyURLsToRestoreOnStartup,
                    prefs::kURLsToRestoreOnStartup),
        TypeAndName(ConfigurationPolicyStore::kPolicyExtensionInstallAllowList,
                    prefs::kExtensionInstallAllowList),
        TypeAndName(ConfigurationPolicyStore::kPolicyExtensionInstallDenyList,
                    prefs::kExtensionInstallDenyList),
        TypeAndName(ConfigurationPolicyStore::kPolicyDisabledPlugins,
                    prefs::kPluginsPluginsBlacklist)));

// Test cases for string-valued policy settings.
class ConfigurationPolicyPrefStoreStringTest
    : public testing::TestWithParam<TypeAndName> {
};

TEST_P(ConfigurationPolicyPrefStoreStringTest, GetDefault) {
  ConfigurationPolicyPrefStore store(NULL, NULL);
  std::string result;
  EXPECT_FALSE(store.prefs()->GetString(GetParam().pref_name(), &result));
}

TEST_P(ConfigurationPolicyPrefStoreStringTest, SetValue) {
  ConfigurationPolicyPrefStore store(NULL, NULL);
  store.Apply(GetParam().type(),
              Value::CreateStringValue("http://chromium.org"));
  std::string result;
  EXPECT_TRUE(store.prefs()->GetString(GetParam().pref_name(), &result));
  EXPECT_EQ(result, "http://chromium.org");
}

INSTANTIATE_TEST_CASE_P(
    ConfigurationPolicyPrefStoreStringTestInstance,
    ConfigurationPolicyPrefStoreStringTest,
    testing::Values(
        TypeAndName(ConfigurationPolicyStore::kPolicyHomePage,
                    prefs::kHomePage),
        TypeAndName(ConfigurationPolicyStore::kPolicyProxyServer,
                    prefs::kProxyServer),
        TypeAndName(ConfigurationPolicyStore::kPolicyProxyPacUrl,
                    prefs::kProxyPacUrl),
        TypeAndName(ConfigurationPolicyStore::kPolicyProxyBypassList,
                    prefs::kProxyBypassList),
        TypeAndName(ConfigurationPolicyStore::kPolicyApplicationLocale,
                    prefs::kApplicationLocale)));

// Test cases for boolean-valued policy settings.
class ConfigurationPolicyPrefStoreBooleanTest
    : public testing::TestWithParam<TypeAndName> {
};

TEST_P(ConfigurationPolicyPrefStoreBooleanTest, GetDefault) {
  ConfigurationPolicyPrefStore store(NULL, NULL);
  bool result = false;
  EXPECT_FALSE(store.prefs()->GetBoolean(GetParam().pref_name(), &result));
}

TEST_P(ConfigurationPolicyPrefStoreBooleanTest, SetValue) {
  ConfigurationPolicyPrefStore store(NULL, NULL);
  store.Apply(GetParam().type(), Value::CreateBooleanValue(false));
  bool result = true;
  EXPECT_TRUE(store.prefs()->GetBoolean(GetParam().pref_name(), &result));
  EXPECT_FALSE(result);

  store.Apply(GetParam().type(), Value::CreateBooleanValue(true));
  result = false;
  EXPECT_TRUE(store.prefs()->GetBoolean(GetParam().pref_name(), &result));
  EXPECT_TRUE(result);
}

INSTANTIATE_TEST_CASE_P(
    ConfigurationPolicyPrefStoreBooleanTestInstance,
    ConfigurationPolicyPrefStoreBooleanTest,
    testing::Values(
        TypeAndName(ConfigurationPolicyStore::kPolicyHomepageIsNewTabPage,
                    prefs::kHomePageIsNewTabPage),
        TypeAndName(ConfigurationPolicyStore::kPolicyAlternateErrorPagesEnabled,
                    prefs::kAlternateErrorPagesEnabled),
        TypeAndName(ConfigurationPolicyStore::kPolicySearchSuggestEnabled,
                    prefs::kSearchSuggestEnabled),
        TypeAndName(ConfigurationPolicyStore::kPolicyDnsPrefetchingEnabled,
                    prefs::kDnsPrefetchingEnabled),
        TypeAndName(ConfigurationPolicyStore::kPolicySafeBrowsingEnabled,
                    prefs::kSafeBrowsingEnabled),
        TypeAndName(ConfigurationPolicyStore::kPolicyMetricsReportingEnabled,
                    prefs::kMetricsReportingEnabled),
        TypeAndName(ConfigurationPolicyStore::kPolicyPasswordManagerEnabled,
                    prefs::kPasswordManagerEnabled),
        TypeAndName(ConfigurationPolicyStore::
                        kPolicyPasswordManagerAllowShowPasswords,
                    prefs::kPasswordManagerAllowShowPasswords),
        TypeAndName(ConfigurationPolicyStore::kPolicyShowHomeButton,
                    prefs::kShowHomeButton),
        TypeAndName(ConfigurationPolicyStore::kPolicyPrintingEnabled,
                    prefs::kPrintingEnabled),
        TypeAndName(ConfigurationPolicyStore::kPolicyJavascriptEnabled,
                    prefs::kWebKitJavascriptEnabled)));

// Test cases for integer-valued policy settings.
class ConfigurationPolicyPrefStoreIntegerTest
    : public testing::TestWithParam<TypeAndName> {
};

TEST_P(ConfigurationPolicyPrefStoreIntegerTest, GetDefault) {
  ConfigurationPolicyPrefStore store(NULL, NULL);
  int result = 0;
  EXPECT_FALSE(store.prefs()->GetInteger(GetParam().pref_name(), &result));
}

TEST_P(ConfigurationPolicyPrefStoreIntegerTest, SetValue) {
  ConfigurationPolicyPrefStore store(NULL, NULL);
  store.Apply(GetParam().type(), Value::CreateIntegerValue(2));
  int result = 0;
  EXPECT_TRUE(store.prefs()->GetInteger(GetParam().pref_name(), &result));
  EXPECT_EQ(result, 2);
}

INSTANTIATE_TEST_CASE_P(
    ConfigurationPolicyPrefStoreIntegerTestInstance,
    ConfigurationPolicyPrefStoreIntegerTest,
    testing::Values(
        TypeAndName(ConfigurationPolicyStore::kPolicyRestoreOnStartup,
                    prefs::kRestoreOnStartup)));

// Test cases for the proxy policy settings.
class ConfigurationPolicyPrefStoreProxyTest : public testing::Test {
};

TEST_F(ConfigurationPolicyPrefStoreProxyTest, CommandLine) {
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
  EXPECT_TRUE(store.prefs()->GetBoolean(prefs::kProxyAutoDetect, &bool_result));
  EXPECT_TRUE(bool_result);
}

TEST_F(ConfigurationPolicyPrefStoreProxyTest, ManualOverride) {
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

TEST_F(ConfigurationPolicyPrefStoreProxyTest, NoProxy) {
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

TEST_F(ConfigurationPolicyPrefStoreProxyTest, NoProxyReversedApplyOrder) {
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

TEST_F(ConfigurationPolicyPrefStoreProxyTest, AutoDetect) {
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

TEST_F(ConfigurationPolicyPrefStoreProxyTest, UseSystem) {
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

TEST_F(ConfigurationPolicyPrefStoreProxyTest, UseSystemReversedApplyOrder) {
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

class ConfigurationPolicyPrefStoreDefaultSearchTest : public testing::Test {
};

// Checks that if the policy for default search is valid, i.e. there's a
// search URL, that all the elements have been given proper defaults.
TEST_F(ConfigurationPolicyPrefStoreDefaultSearchTest, MinimallyDefined) {
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
TEST_F(ConfigurationPolicyPrefStoreDefaultSearchTest, FullyDefined) {
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
TEST_F(ConfigurationPolicyPrefStoreDefaultSearchTest, MissingUrl) {
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
TEST_F(ConfigurationPolicyPrefStoreDefaultSearchTest, Invalid) {
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

// Test cases for the Sync policy setting.
class ConfigurationPolicyPrefStoreSyncTest : public testing::Test {
};

TEST_F(ConfigurationPolicyPrefStoreSyncTest, Default) {
  ConfigurationPolicyPrefStore store(NULL, NULL);
  bool result = false;
  EXPECT_FALSE(store.prefs()->GetBoolean(prefs::kSyncManaged, &result));
}

TEST_F(ConfigurationPolicyPrefStoreSyncTest, Enabled) {
  ConfigurationPolicyPrefStore store(NULL, NULL);
  store.Apply(ConfigurationPolicyPrefStore::kPolicySyncDisabled,
              Value::CreateBooleanValue(false));
  // Enabling Sync should not set the pref.
  bool result = false;
  EXPECT_FALSE(store.prefs()->GetBoolean(prefs::kSyncManaged, &result));
}

TEST_F(ConfigurationPolicyPrefStoreSyncTest, Disabled) {
  ConfigurationPolicyPrefStore store(NULL, NULL);
  store.Apply(ConfigurationPolicyPrefStore::kPolicySyncDisabled,
              Value::CreateBooleanValue(true));
  // Sync should be flagged as managed.
  bool result = false;
  EXPECT_TRUE(store.prefs()->GetBoolean(prefs::kSyncManaged, &result));
  EXPECT_TRUE(result);
}

// Test cases for the AutoFill policy setting.
class ConfigurationPolicyPrefStoreAutoFillTest : public testing::Test {
};

TEST_F(ConfigurationPolicyPrefStoreAutoFillTest, Default) {
  ConfigurationPolicyPrefStore store(NULL, NULL);
  bool result = false;
  EXPECT_FALSE(store.prefs()->GetBoolean(prefs::kAutoFillEnabled, &result));
}

TEST_F(ConfigurationPolicyPrefStoreAutoFillTest, Enabled) {
  ConfigurationPolicyPrefStore store(NULL, NULL);
  store.Apply(ConfigurationPolicyPrefStore::kPolicyAutoFillEnabled,
              Value::CreateBooleanValue(true));
  // Enabling AutoFill should not set the pref.
  bool result = false;
  EXPECT_FALSE(store.prefs()->GetBoolean(prefs::kAutoFillEnabled, &result));
}

TEST_F(ConfigurationPolicyPrefStoreAutoFillTest, Disabled) {
  ConfigurationPolicyPrefStore store(NULL, NULL);
  store.Apply(ConfigurationPolicyPrefStore::kPolicyAutoFillEnabled,
              Value::CreateBooleanValue(false));
  // Disabling AutoFill should switch the pref to managed.
  bool result = true;
  EXPECT_TRUE(store.prefs()->GetBoolean(prefs::kAutoFillEnabled, &result));
  EXPECT_FALSE(result);
}

}  // namespace policy
