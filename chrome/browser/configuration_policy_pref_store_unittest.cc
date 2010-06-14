// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "chrome/browser/configuration_policy_pref_store.h"
#include "chrome/common/pref_names.h"

class ConfigurationPolicyPrefStoreTest : public testing::Test {
 public:
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

void ConfigurationPolicyPrefStoreTest::TestStringPolicyGetDefault(
    const wchar_t* pref_name) {
  ConfigurationPolicyPrefStore store(0);
  std::wstring result;
  store.prefs()->GetString(pref_name, &result);
  EXPECT_EQ(result, L"");
}

void ConfigurationPolicyPrefStoreTest::TestStringPolicySetValue(
    const wchar_t* pref_name, ConfigurationPolicyStore::PolicyType type) {
  ConfigurationPolicyPrefStore store(0);
  store.Apply(type, Value::CreateStringValue("http://chromium.org"));
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
  ConfigurationPolicyPrefStore store(0);
  bool result = false;
  store.prefs()->GetBoolean(pref_name, &result);
  EXPECT_FALSE(result);
  result = true;
  store.prefs()->GetBoolean(pref_name, &result);
  EXPECT_TRUE(result);
}

void ConfigurationPolicyPrefStoreTest::TestBooleanPolicySetValue(
    const wchar_t* pref_name, ConfigurationPolicyStore::PolicyType type) {
  ConfigurationPolicyPrefStore store(0);
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
  ConfigurationPolicyPrefStore store(0);
  int result = 0;
  store.prefs()->GetInteger(pref_name, &result);
  EXPECT_EQ(result, 0);
}

void ConfigurationPolicyPrefStoreTest::TestIntegerPolicySetValue(
    const wchar_t* pref_name, ConfigurationPolicyStore::PolicyType type) {
  ConfigurationPolicyPrefStore store(0);
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

TEST_F(ConfigurationPolicyPrefStoreTest, TestSettingCookiesEnabledDefault) {
  TestIntegerPolicy(prefs::kCookieBehavior,
      ConfigurationPolicyPrefStore::kPolicyCookiesMode);
}

