// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signed_settings.h"

#include <map>
#include <string>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/cros_settings_names.h"
#include "chrome/browser/chromeos/login/mock_user_manager.h"
#include "chrome/browser/chromeos/login/signed_settings_cache.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::Return;

namespace em = enterprise_management;
namespace chromeos {

class CrosSettingsTest : public testing::Test {
 protected:
  CrosSettingsTest()
      : message_loop_(MessageLoop::TYPE_UI),
        ui_thread_(content::BrowserThread::UI, &message_loop_),
        file_thread_(content::BrowserThread::FILE, &message_loop_),
        pointer_factory_(this),
        local_state_(static_cast<TestingBrowserProcess*>(g_browser_process)) {
  }

  virtual ~CrosSettingsTest() {
  }

  virtual void SetUp() {
    EXPECT_CALL(*mock_user_manager_.user_manager(), IsCurrentUserOwner())
        .Times(AnyNumber())
        .WillRepeatedly(Return(true));
    // Reset the cache between tests.
    ApplyEmptyPolicy();
  }

  virtual void TearDown() {
    message_loop_.RunAllPending();
    ASSERT_TRUE(expected_props_.empty());
    // Reset the cache between tests.
    ApplyEmptyPolicy();
    STLDeleteValues(&expected_props_);
  }

  void FetchPref(const std::string& pref) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    if (expected_props_.find(pref) == expected_props_.end())
      return;

    if (CrosSettingsProvider::TRUSTED ==
        CrosSettings::Get()->PrepareTrustedValues(
            base::Bind(&CrosSettingsTest::FetchPref,
                       pointer_factory_.GetWeakPtr(), pref))) {
      scoped_ptr<base::Value> expected_value(
          expected_props_.find(pref)->second);
      const base::Value* pref_value = CrosSettings::Get()->GetPref(pref);
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
    CrosSettings::Get()->Set(pref_name, *value);
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
    policy->set_policy_type(chromeos::kDevicePolicyType);
    policy->set_username("me@owner");
    policy->set_policy_value(pol.SerializeAsString());
    // Wipe the signed settings store.
    response.set_policy_data(policy->SerializeAsString());
    response.set_policy_data_signature("false");
  }

  void ApplyEmptyPolicy() {
    em::PolicyData fake_pol;
    PrepareEmptyPolicy(&fake_pol);
    signed_settings_cache::Store(fake_pol, local_state_.Get());
    CrosSettings::Get()->ReloadProviders();
  }

  std::map<std::string, base::Value*> expected_props_;

  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  base::WeakPtrFactory<CrosSettingsTest> pointer_factory_;

  ScopedTestingLocalState local_state_;

  ScopedMockUserManagerEnabler mock_user_manager_;
  ScopedStubCrosEnabler stub_cros_enabler_;
};

TEST_F(CrosSettingsTest, SetPref) {
  // Change to something that is not the default.
  AddExpectation(kAccountsPrefAllowGuest,
                 base::Value::CreateBooleanValue(false));
  SetPref(kAccountsPrefAllowGuest, expected_props_[kAccountsPrefAllowGuest]);
  FetchPref(kAccountsPrefAllowGuest);
  message_loop_.RunAllPending();
  ASSERT_TRUE(expected_props_.empty());
}

TEST_F(CrosSettingsTest, GetPref) {
  // We didn't change the default so look for it.
  AddExpectation(kAccountsPrefAllowGuest,
                 base::Value::CreateBooleanValue(true));
  FetchPref(kAccountsPrefAllowGuest);
}

TEST_F(CrosSettingsTest, SetWhitelist) {
  // Setting the whitelist should also switch the value of
  // kAccountsPrefAllowNewUser to false.
  base::ListValue whitelist;
  whitelist.Append(base::Value::CreateStringValue("me@owner"));
  AddExpectation(kAccountsPrefAllowNewUser,
                 base::Value::CreateBooleanValue(false));
  AddExpectation(kAccountsPrefUsers, whitelist.DeepCopy());
  SetPref(kAccountsPrefUsers, &whitelist);
  FetchPref(kAccountsPrefAllowNewUser);
  FetchPref(kAccountsPrefUsers);
}

TEST_F(CrosSettingsTest, SetWhitelistWithListOps) {
  base::ListValue* whitelist = new base::ListValue();
  base::StringValue hacky_user("h@xxor");
  whitelist->Append(hacky_user.DeepCopy());
  AddExpectation(kAccountsPrefAllowNewUser,
                 base::Value::CreateBooleanValue(false));
  AddExpectation(kAccountsPrefUsers, whitelist);
  // Add some user to the whitelist.
  CrosSettings::Get()->AppendToList(kAccountsPrefUsers, &hacky_user);
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
  AddExpectation(kAccountsPrefAllowNewUser,
                 base::Value::CreateBooleanValue(false));
  AddExpectation(kAccountsPrefUsers, whitelist.DeepCopy());
  SetPref(kAccountsPrefUsers, &whitelist);
  FetchPref(kAccountsPrefAllowNewUser);
  FetchPref(kAccountsPrefUsers);
  message_loop_.RunAllPending();
  ASSERT_TRUE(expected_props_.empty());
  // Now try to remove one element from that list.
  AddExpectation(kAccountsPrefUsers, expected_list);
  CrosSettings::Get()->RemoveFromList(kAccountsPrefUsers, &lamy_user);
  FetchPref(kAccountsPrefAllowNewUser);
  FetchPref(kAccountsPrefUsers);
}

TEST_F(CrosSettingsTest, SetEmptyWhitelist) {
  // Setting the whitelist empty should switch the value of
  // kAccountsPrefAllowNewUser to true.
  base::ListValue whitelist;
  base::FundamentalValue disallow_new(false);
  AddExpectation(kAccountsPrefAllowNewUser,
                 base::Value::CreateBooleanValue(true));
  SetPref(kAccountsPrefUsers, &whitelist);
  SetPref(kAccountsPrefAllowNewUser, &disallow_new);
  FetchPref(kAccountsPrefAllowNewUser);
  FetchPref(kAccountsPrefUsers);
}

TEST_F(CrosSettingsTest, SetWhitelistAndNoNewUsers) {
  // Setting the whitelist should allow us to set kAccountsPrefAllowNewUser to
  // false (which is the implicit value too).
  base::ListValue whitelist;
  whitelist.Append(base::Value::CreateStringValue("me@owner"));
  AddExpectation(kAccountsPrefUsers, whitelist.DeepCopy());
  AddExpectation(kAccountsPrefAllowNewUser,
                 base::Value::CreateBooleanValue(false));
  SetPref(kAccountsPrefUsers, &whitelist);
  SetPref(kAccountsPrefAllowNewUser,
          expected_props_[kAccountsPrefAllowNewUser]);
  FetchPref(kAccountsPrefAllowNewUser);
  FetchPref(kAccountsPrefUsers);
}

TEST_F(CrosSettingsTest, SetAllowNewUsers) {
  // Setting kAccountsPrefAllowNewUser to true with no whitelist should be ok.
  AddExpectation(kAccountsPrefAllowNewUser,
                 base::Value::CreateBooleanValue(true));
  SetPref(kAccountsPrefAllowNewUser,
          expected_props_[kAccountsPrefAllowNewUser]);
  FetchPref(kAccountsPrefAllowNewUser);
}

TEST_F(CrosSettingsTest, SetOwner) {
  base::StringValue hacky_owner("h@xxor");
  AddExpectation(kDeviceOwner, base::Value::CreateStringValue("h@xxor"));
  SetPref(kDeviceOwner, &hacky_owner);
  FetchPref(kDeviceOwner);
}

TEST_F(CrosSettingsTest, SetEphemeralUsersEnabled) {
  base::FundamentalValue ephemeral_users_enabled(true);
  AddExpectation(kAccountsPrefEphemeralUsersEnabled,
                 base::Value::CreateBooleanValue(true));
  SetPref(kAccountsPrefEphemeralUsersEnabled, &ephemeral_users_enabled);
  FetchPref(kAccountsPrefEphemeralUsersEnabled);
}

TEST_F(CrosSettingsTest, FindEmailInList) {
  base::ListValue list;
  list.Append(base::Value::CreateStringValue("user@example.com"));
  list.Append(base::Value::CreateStringValue("nodomain"));
  list.Append(base::Value::CreateStringValue("with.dots@gmail.com"));
  list.Append(base::Value::CreateStringValue("Upper@example.com"));

  CrosSettings* cs = CrosSettings::Get();
  cs->Set(kAccountsPrefUsers, list);

  EXPECT_TRUE(cs->FindEmailInList(kAccountsPrefUsers, "user@example.com"));
  EXPECT_FALSE(cs->FindEmailInList(kAccountsPrefUsers, "us.er@example.com"));
  EXPECT_TRUE(cs->FindEmailInList(kAccountsPrefUsers, "USER@example.com"));
  EXPECT_FALSE(cs->FindEmailInList(kAccountsPrefUsers, "user"));

  EXPECT_TRUE(cs->FindEmailInList(kAccountsPrefUsers, "nodomain"));
  EXPECT_TRUE(cs->FindEmailInList(kAccountsPrefUsers, "nodomain@gmail.com"));
  EXPECT_TRUE(cs->FindEmailInList(kAccountsPrefUsers, "no.domain@gmail.com"));
  EXPECT_TRUE(cs->FindEmailInList(kAccountsPrefUsers, "NO.DOMAIN"));

  EXPECT_TRUE(cs->FindEmailInList(kAccountsPrefUsers, "with.dots@gmail.com"));
  EXPECT_TRUE(cs->FindEmailInList(kAccountsPrefUsers, "withdots@gmail.com"));
  EXPECT_TRUE(cs->FindEmailInList(kAccountsPrefUsers, "WITH.DOTS@gmail.com"));
  EXPECT_TRUE(cs->FindEmailInList(kAccountsPrefUsers, "WITHDOTS"));

  EXPECT_TRUE(cs->FindEmailInList(kAccountsPrefUsers, "Upper@example.com"));
  EXPECT_FALSE(cs->FindEmailInList(kAccountsPrefUsers, "U.pper@example.com"));
  EXPECT_FALSE(cs->FindEmailInList(kAccountsPrefUsers, "Upper"));
  EXPECT_TRUE(cs->FindEmailInList(kAccountsPrefUsers, "upper@example.com"));
}

}  // namespace chromeos
