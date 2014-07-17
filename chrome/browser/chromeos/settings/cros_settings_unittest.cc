// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "content/public/test/test_browser_thread.h"
#include "policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace chromeos {

class CrosSettingsTest : public testing::Test {
 protected:
  CrosSettingsTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        local_state_(TestingBrowserProcess::GetGlobal()),
        settings_(DeviceSettingsService::Get()),
        weak_factory_(this) {}

  virtual ~CrosSettingsTest() {}

  virtual void TearDown() OVERRIDE {
    ASSERT_TRUE(expected_props_.empty());
    STLDeleteValues(&expected_props_);
    expected_props_.clear();
  }

  void FetchPref(const std::string& pref) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    if (expected_props_.find(pref) == expected_props_.end())
      return;

    if (CrosSettingsProvider::TRUSTED ==
            settings_.PrepareTrustedValues(
                base::Bind(&CrosSettingsTest::FetchPref,
                           weak_factory_.GetWeakPtr(), pref))) {
      scoped_ptr<base::Value> expected_value(
          expected_props_.find(pref)->second);
      const base::Value* pref_value = settings_.GetPref(pref);
      if (expected_value.get()) {
        ASSERT_TRUE(pref_value);
        ASSERT_TRUE(expected_value->Equals(pref_value));
      } else {
        ASSERT_FALSE(pref_value);
      }
      expected_props_.erase(pref);
    }
  }

  void SetPref(const std::string& pref_name, const base::Value* value) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    settings_.Set(pref_name, *value);
  }

  void AddExpectation(const std::string& pref_name, base::Value* value) {
    base::Value*& entry = expected_props_[pref_name];
    delete entry;
    entry = value;
  }

  void PrepareEmptyPolicy(em::PolicyData* policy) {
    // Prepare some policy blob.
    em::PolicyFetchResponse response;
    em::ChromeDeviceSettingsProto pol;
    policy->set_policy_type(policy::dm_protocol::kChromeDevicePolicyType);
    policy->set_username("me@owner");
    policy->set_policy_value(pol.SerializeAsString());
    // Wipe the signed settings store.
    response.set_policy_data(policy->SerializeAsString());
    response.set_policy_data_signature("false");
  }

  static bool IsWhitelisted(CrosSettings* cs, const std::string& username) {
    return cs->FindEmailInList(kAccountsPrefUsers, username, NULL);
  }

  base::MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;

  ScopedTestingLocalState local_state_;
  ScopedDeviceSettingsTestHelper device_settings_test_helper_;
  CrosSettings settings_;

  base::WeakPtrFactory<CrosSettingsTest> weak_factory_;

  std::map<std::string, base::Value*> expected_props_;
};

TEST_F(CrosSettingsTest, SetPref) {
  // Change to something that is not the default.
  AddExpectation(kAccountsPrefAllowGuest, new base::FundamentalValue(false));
  SetPref(kAccountsPrefAllowGuest, expected_props_[kAccountsPrefAllowGuest]);
  FetchPref(kAccountsPrefAllowGuest);
  ASSERT_TRUE(expected_props_.empty());
}

TEST_F(CrosSettingsTest, GetPref) {
  // We didn't change the default so look for it.
  AddExpectation(kAccountsPrefAllowGuest, new base::FundamentalValue(true));
  FetchPref(kAccountsPrefAllowGuest);
}

TEST_F(CrosSettingsTest, SetWhitelist) {
  // Setting the whitelist should also switch the value of
  // kAccountsPrefAllowNewUser to false.
  base::ListValue whitelist;
  whitelist.Append(new base::StringValue("me@owner"));
  AddExpectation(kAccountsPrefAllowNewUser, new base::FundamentalValue(false));
  AddExpectation(kAccountsPrefUsers, whitelist.DeepCopy());
  SetPref(kAccountsPrefUsers, &whitelist);
  FetchPref(kAccountsPrefAllowNewUser);
  FetchPref(kAccountsPrefUsers);
}

TEST_F(CrosSettingsTest, SetWhitelistWithListOps) {
  base::ListValue* whitelist = new base::ListValue();
  base::StringValue hacky_user("h@xxor");
  whitelist->Append(hacky_user.DeepCopy());
  AddExpectation(kAccountsPrefAllowNewUser, new base::FundamentalValue(false));
  AddExpectation(kAccountsPrefUsers, whitelist);
  // Add some user to the whitelist.
  settings_.AppendToList(kAccountsPrefUsers, &hacky_user);
  FetchPref(kAccountsPrefAllowNewUser);
  FetchPref(kAccountsPrefUsers);
}

TEST_F(CrosSettingsTest, SetWhitelistWithListOps2) {
  base::ListValue whitelist;
  base::StringValue hacky_user("h@xxor");
  base::StringValue lamy_user("l@mer");
  whitelist.Append(hacky_user.DeepCopy());
  base::ListValue* expected_list = whitelist.DeepCopy();
  whitelist.Append(lamy_user.DeepCopy());
  AddExpectation(kAccountsPrefAllowNewUser, new base::FundamentalValue(false));
  AddExpectation(kAccountsPrefUsers, whitelist.DeepCopy());
  SetPref(kAccountsPrefUsers, &whitelist);
  FetchPref(kAccountsPrefAllowNewUser);
  FetchPref(kAccountsPrefUsers);
  ASSERT_TRUE(expected_props_.empty());
  // Now try to remove one element from that list.
  AddExpectation(kAccountsPrefUsers, expected_list);
  settings_.RemoveFromList(kAccountsPrefUsers, &lamy_user);
  FetchPref(kAccountsPrefAllowNewUser);
  FetchPref(kAccountsPrefUsers);
}

TEST_F(CrosSettingsTest, SetEmptyWhitelist) {
  // Setting the whitelist empty should switch the value of
  // kAccountsPrefAllowNewUser to true.
  base::ListValue whitelist;
  AddExpectation(kAccountsPrefAllowNewUser, new base::FundamentalValue(true));
  SetPref(kAccountsPrefUsers, &whitelist);
  FetchPref(kAccountsPrefAllowNewUser);
  FetchPref(kAccountsPrefUsers);
}

TEST_F(CrosSettingsTest, SetEmptyWhitelistAndNoNewUsers) {
  // Setting the whitelist empty and disallowing new users should result in no
  // new users allowed.
  base::ListValue whitelist;
  base::FundamentalValue disallow_new(false);
  AddExpectation(kAccountsPrefUsers, whitelist.DeepCopy());
  AddExpectation(kAccountsPrefAllowNewUser, new base::FundamentalValue(false));
  SetPref(kAccountsPrefUsers, &whitelist);
  SetPref(kAccountsPrefAllowNewUser, &disallow_new);
  FetchPref(kAccountsPrefAllowNewUser);
  FetchPref(kAccountsPrefUsers);
}

TEST_F(CrosSettingsTest, SetWhitelistAndNoNewUsers) {
  // Setting the whitelist should allow us to set kAccountsPrefAllowNewUser to
  // false (which is the implicit value too).
  base::ListValue whitelist;
  whitelist.Append(new base::StringValue("me@owner"));
  AddExpectation(kAccountsPrefUsers, whitelist.DeepCopy());
  AddExpectation(kAccountsPrefAllowNewUser, new base::FundamentalValue(false));
  SetPref(kAccountsPrefUsers, &whitelist);
  SetPref(kAccountsPrefAllowNewUser,
          expected_props_[kAccountsPrefAllowNewUser]);
  FetchPref(kAccountsPrefAllowNewUser);
  FetchPref(kAccountsPrefUsers);
}

TEST_F(CrosSettingsTest, SetAllowNewUsers) {
  // Setting kAccountsPrefAllowNewUser to true with no whitelist should be ok.
  AddExpectation(kAccountsPrefAllowNewUser, new base::FundamentalValue(true));
  SetPref(kAccountsPrefAllowNewUser,
          expected_props_[kAccountsPrefAllowNewUser]);
  FetchPref(kAccountsPrefAllowNewUser);
}

TEST_F(CrosSettingsTest, SetEphemeralUsersEnabled) {
  base::FundamentalValue ephemeral_users_enabled(true);
  AddExpectation(kAccountsPrefEphemeralUsersEnabled,
                 new base::FundamentalValue(true));
  SetPref(kAccountsPrefEphemeralUsersEnabled, &ephemeral_users_enabled);
  FetchPref(kAccountsPrefEphemeralUsersEnabled);
}

TEST_F(CrosSettingsTest, FindEmailInList) {
  base::ListValue list;
  list.Append(new base::StringValue("user@example.com"));
  list.Append(new base::StringValue("nodomain"));
  list.Append(new base::StringValue("with.dots@gmail.com"));
  list.Append(new base::StringValue("Upper@example.com"));

  CrosSettings* cs = &settings_;
  cs->Set(kAccountsPrefUsers, list);

  EXPECT_TRUE(IsWhitelisted(cs, "user@example.com"));
  EXPECT_FALSE(IsWhitelisted(cs, "us.er@example.com"));
  EXPECT_TRUE(IsWhitelisted(cs, "USER@example.com"));
  EXPECT_FALSE(IsWhitelisted(cs, "user"));

  EXPECT_TRUE(IsWhitelisted(cs, "nodomain"));
  EXPECT_TRUE(IsWhitelisted(cs, "nodomain@gmail.com"));
  EXPECT_TRUE(IsWhitelisted(cs, "no.domain@gmail.com"));
  EXPECT_TRUE(IsWhitelisted(cs, "NO.DOMAIN"));

  EXPECT_TRUE(IsWhitelisted(cs, "with.dots@gmail.com"));
  EXPECT_TRUE(IsWhitelisted(cs, "withdots@gmail.com"));
  EXPECT_TRUE(IsWhitelisted(cs, "WITH.DOTS@gmail.com"));
  EXPECT_TRUE(IsWhitelisted(cs, "WITHDOTS"));

  EXPECT_TRUE(IsWhitelisted(cs, "Upper@example.com"));
  EXPECT_FALSE(IsWhitelisted(cs, "U.pper@example.com"));
  EXPECT_FALSE(IsWhitelisted(cs, "Upper"));
  EXPECT_TRUE(IsWhitelisted(cs, "upper@example.com"));
}

TEST_F(CrosSettingsTest, FindEmailInListWildcard) {
  base::ListValue list;
  list.Append(new base::StringValue("user@example.com"));
  list.Append(new base::StringValue("*@example.com"));

  CrosSettings* cs = &settings_;
  cs->Set(kAccountsPrefUsers, list);

  bool wildcard_match = false;
  EXPECT_TRUE(cs->FindEmailInList(
      kAccountsPrefUsers, "test@example.com", &wildcard_match));
  EXPECT_TRUE(wildcard_match);
  EXPECT_TRUE(cs->FindEmailInList(
      kAccountsPrefUsers, "user@example.com", &wildcard_match));
  EXPECT_FALSE(wildcard_match);
  EXPECT_TRUE(cs->FindEmailInList(
      kAccountsPrefUsers, "*@example.com", &wildcard_match));
  EXPECT_TRUE(wildcard_match);
}

}  // namespace chromeos
