// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signed_settings_cache.h"

#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace chromeos {

class SignedSettingsCacheTest : public testing::Test {
 protected:
  virtual void SetUp() {
    // prepare some data.
    policy_.set_policy_type("google/chromeos/device");
    em::ChromeDeviceSettingsProto pol;
    pol.mutable_allow_new_users()->set_allow_new_users(false);
    policy_.set_policy_value(pol.SerializeAsString());

    signed_settings_cache::RegisterPrefs(&local_state_);
  }

  TestingPrefService local_state_;
  em::PolicyData policy_;
};

TEST_F(SignedSettingsCacheTest, Basic) {
  EXPECT_TRUE(signed_settings_cache::Store(policy_, &local_state_));

  em::PolicyData policy_out;
  EXPECT_TRUE(signed_settings_cache::Retrieve(&policy_out, &local_state_));

  EXPECT_TRUE(policy_out.has_policy_type());
  EXPECT_TRUE(policy_out.has_policy_value());

  em::ChromeDeviceSettingsProto pol;
  pol.ParseFromString(policy_out.policy_value());
  EXPECT_TRUE(pol.has_allow_new_users());
  EXPECT_FALSE(pol.allow_new_users().allow_new_users());
}

TEST_F(SignedSettingsCacheTest, CorruptData) {
  EXPECT_TRUE(signed_settings_cache::Store(policy_, &local_state_));

  local_state_.SetString(prefs::kSignedSettingsCache, "blaaa");

  em::PolicyData policy_out;
  EXPECT_FALSE(signed_settings_cache::Retrieve(&policy_out, &local_state_));
}

}  // namespace chromeos
