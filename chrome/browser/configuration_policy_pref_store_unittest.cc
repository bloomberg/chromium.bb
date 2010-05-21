// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "chrome/browser/configuration_policy_pref_store.h"
#include "chrome/common/pref_names.h"

TEST(ConfigurationPolicyPrefStoreTest, TestSettingHomePageDefault) {
  ConfigurationPolicyPrefStore store(0);
  std::wstring result;
  store.prefs()->GetString(prefs::kHomePage, &result);
  EXPECT_EQ(result, L"");
}

TEST(ConfigurationPolicyPrefStoreTest, TestSettingHomePageOverride) {
  ConfigurationPolicyPrefStore store(0);
  store.Apply(ConfigurationPolicyPrefStore::kPolicyHomePage,
              Value::CreateStringValue("http://chromium.org"));
  std::wstring result;
  store.prefs()->GetString(prefs::kHomePage, &result);
  EXPECT_EQ(result, L"http://chromium.org");
}

TEST(ConfigurationPolicyPrefStoreTest, TestSettingHomepageIsNewTabPageDefault) {
  ConfigurationPolicyPrefStore store(0);
  bool result = false;
  store.prefs()->GetBoolean(prefs::kHomePageIsNewTabPage, &result);
  EXPECT_FALSE(result);
  result = true;
  store.prefs()->GetBoolean(prefs::kHomePageIsNewTabPage, &result);
  EXPECT_TRUE(result);
}

TEST(ConfigurationPolicyPrefStoreTest, TestSettingHomepageIsNewTabPage) {
  ConfigurationPolicyPrefStore store(0);
  store.Apply(ConfigurationPolicyPrefStore::kPolicyHomepageIsNewTabPage,
              Value::CreateBooleanValue(false));
  bool result = true;
  store.prefs()->GetBoolean(prefs::kHomePageIsNewTabPage, &result);
  EXPECT_FALSE(result);

  store.Apply(ConfigurationPolicyPrefStore::kPolicyHomepageIsNewTabPage,
              Value::CreateBooleanValue(true));
  store.prefs()->GetBoolean(prefs::kHomePageIsNewTabPage, &result);
  EXPECT_TRUE(result);
}

TEST(ConfigurationPolicyPrefStoreTest, TestSettingCookiesEnabledDefault) {
  ConfigurationPolicyPrefStore store(0);
  int result = 0;
  store.prefs()->GetInteger(prefs::kCookieBehavior, &result);
  EXPECT_EQ(result, 0);
}

TEST(ConfigurationPolicyPrefStoreTest, TestSettingCookiesEnabledOverride) {
  ConfigurationPolicyPrefStore store(0);
  store.Apply(ConfigurationPolicyPrefStore::kPolicyCookiesMode,
              Value::CreateIntegerValue(2));
  int result = 0;
  store.prefs()->GetInteger(prefs::kCookieBehavior, &result);
  EXPECT_EQ(result, 2);
}

