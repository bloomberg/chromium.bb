// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/test/data/resource.h"
#include "base/registry.h"
#include "base/string_util.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/group_policy.h"
#include "chrome/browser/group_policy_settings.h"
#include "chrome/installer/util/browser_distribution.h"
#include "testing/gtest/include/gtest/gtest.h"

class GroupPolicyTest : public testing::Test {
 protected:
  virtual void SetUp() {
    std::wstring policy_key(L"");
    group_policy::GetPolicySettingsRootKey(&policy_key);
    policy_key.append(L"\\Preferences\\");
    RegKey hkcu = RegKey(HKEY_CURRENT_USER, policy_key.c_str(), KEY_WRITE);
    RegKey hklm = RegKey(HKEY_LOCAL_MACHINE, policy_key.c_str(), KEY_WRITE);
    EXPECT_TRUE(hkcu.Valid());
    EXPECT_TRUE(hklm.Valid());
    EXPECT_TRUE(hkcu.WriteValue(L"Homepage", L"www.google.com/hkcu"));
    EXPECT_TRUE(hklm.WriteValue(L"Homepage", L"www.google.com/hklm"));
    EXPECT_TRUE(hkcu.WriteValue(L"HomepageIsNewTabPage", (DWORD)0));
    EXPECT_TRUE(hklm.WriteValue(L"HomepageIsNewTabPage", (DWORD)1));
    policy_key.clear();
    group_policy::GetPolicySettingsRootKey(&policy_key);
    policy_key.append(L"\\ChromeFrameDomainWhiteList\\");
    RegKey hkcu_list = RegKey(HKEY_CURRENT_USER,
                              policy_key.c_str(),
                              KEY_WRITE);
    RegKey hklm_list = RegKey(HKEY_LOCAL_MACHINE,
                              policy_key.c_str(),
                              KEY_WRITE);
    EXPECT_TRUE(hkcu_list.Valid());
    EXPECT_TRUE(hklm_list.Valid());
    EXPECT_TRUE(hkcu_list.WriteValue(L"www.google.com/test1",
                                     L"www.google.com/test1"));
    EXPECT_TRUE(hkcu_list.WriteValue(L"www.google.com/test2",
                                     L"www.google.com/test2"));
    EXPECT_TRUE(hkcu_list.WriteValue(L"www.google.com/test3",
                                     L"www.google.com/test3"));
    EXPECT_TRUE(hklm_list.WriteValue(L"www.google.com/testa",
                                     L"www.google.com/testa"));
    EXPECT_TRUE(hklm_list.WriteValue(L"www.google.com/testb",
                                     L"www.google.com/testb"));
    EXPECT_TRUE(hklm_list.WriteValue(L"www.google.com/testc",
                                     L"www.google.com/testc"));
  }

  virtual void TearDown() {
    // Delete the appropriate registry keys.
    std::wstring policy_key(L"");
    group_policy::GetPolicySettingsRootKey(&policy_key);
    policy_key.append(L"\\Preferences\\");
    RegKey hkcu = RegKey(HKEY_CURRENT_USER, policy_key.c_str(), KEY_WRITE);
    RegKey hklm = RegKey(HKEY_LOCAL_MACHINE, policy_key.c_str(), KEY_WRITE);
    EXPECT_TRUE(hkcu.Valid());
    EXPECT_TRUE(hklm.Valid());
    hkcu.DeleteValue(L"Homepage");
    hklm.DeleteValue(L"Homepage");
    hklm.DeleteValue(L"HomepageIsNewTabPage");
    hkcu.DeleteValue(L"HomepageIsNewTabPage");
    policy_key.clear();
    group_policy::GetPolicySettingsRootKey(&policy_key);
    RegKey hkcu_list = RegKey(HKEY_CURRENT_USER,
                              policy_key.c_str(),
                              KEY_WRITE);
    RegKey hklm_list = RegKey(HKEY_LOCAL_MACHINE,
                              policy_key.c_str(),
                              KEY_WRITE);
    EXPECT_TRUE(hkcu_list.DeleteKey(L"ChromeFrameDomainWhiteList"));
  }
};

TEST_F(GroupPolicyTest, TestGetPolicyKey) {
  std::wstring mystring(L"");
  group_policy::GetPolicySettingsRootKey(&mystring);
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::wstring app_name = dist->GetApplicationName();
  std::wstring publisher_name = dist->GetPublisherName();
  EXPECT_NE(-1, mystring.find(app_name));
  EXPECT_NE(-1, mystring.find(publisher_name));
}

TEST_F(GroupPolicyTest, TestGetGroupPolicyString) {
  std::wstring setting = std::wstring(L"");
  bool found = false;
  EXPECT_TRUE(group_policy::gpHomepage.IsPolicyControlled());
  HRESULT hr = group_policy::gpHomepage.GetSetting(&setting, &found);
  EXPECT_TRUE(found);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(0, setting.compare(L"www.google.com/hklm"));
}

TEST_F(GroupPolicyTest, TestGetGroupPolicyBool) {
  bool setting = false;
  bool found = false;
  EXPECT_TRUE(group_policy::gpHomepageIsNewTabPage.IsPolicyControlled());
  HRESULT hr = group_policy::gpHomepageIsNewTabPage.GetSetting(&setting,
                                                               &found);
  EXPECT_TRUE(found);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_TRUE(setting);
}

TEST_F(GroupPolicyTest, TestGetStringList) {
  scoped_ptr<ListValue> mixed_list(new ListValue());
  bool found = false;
  // Because of the way the lists of strings are appended,
  // HKCU appears before HKLM, and they appear in reverse order.
  std::string test1 = std::string("www.google.com/testc");
  std::string test2 = std::string("www.google.com/testb");
  std::string test3 = std::string("www.google.com/testa");
  std::string test4 = std::string("www.google.com/test3");
  std::string test5 = std::string("www.google.com/test2");
  std::string test6 = std::string("www.google.com/test1");
  HRESULT hr = \
      group_policy::gpChromeFrameDomainWhiteList.GetSetting(mixed_list.get(),
                                                            &found);
  EXPECT_TRUE(found);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(6, mixed_list->GetSize());
  std::string string_value;
  EXPECT_TRUE(mixed_list->GetString(0, &string_value));
  EXPECT_EQ(0, string_value.compare(test1));
  EXPECT_EQ(string_value, test1);

  EXPECT_TRUE(mixed_list->GetString(1, &string_value));
  EXPECT_EQ(0, string_value.compare(test2));
  EXPECT_EQ(string_value, test2);

  EXPECT_TRUE(mixed_list->GetString(2, &string_value));
  EXPECT_EQ(0, string_value.compare(test3));
  EXPECT_EQ(string_value, test3);

  EXPECT_TRUE(mixed_list->GetString(3, &string_value));
  EXPECT_EQ(0, string_value.compare(test4));
  EXPECT_EQ(string_value, test4);

  EXPECT_TRUE(mixed_list->GetString(4, &string_value));
  EXPECT_EQ(0, string_value.compare(test5));
  EXPECT_EQ(string_value, test5);

  EXPECT_TRUE(mixed_list->GetString(5, &string_value));
  EXPECT_EQ(0, string_value.compare(test6));
  EXPECT_EQ(string_value, test6);
}

