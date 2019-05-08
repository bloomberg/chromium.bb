// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"

#include "base/json/json_reader.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plugin_vm {

class PluginVmUtilTest : public testing::Test {
 public:
  PluginVmUtilTest() = default;

  void SetUp() override {
    settings_helper_.ReplaceDeviceSettingsProviderWithStub();
    profile_builder.SetProfileName("user0");
    testing_profile_ = profile_builder.Build();
  }

  void TearDown() override {
    settings_helper_.RestoreRealDeviceSettingsProvider();
    testing_profile_.reset();
  }

 protected:
  TestingProfile::Builder profile_builder;
  chromeos::ScopedCrosSettingsTestHelper settings_helper_;
  std::unique_ptr<TestingProfile> testing_profile_;
  content::TestBrowserThreadBundle thread_bundle_;
  chromeos::MockUserManager user_manager_;

  void SetPolicyRequirementsToAllowPluginVm() {
    settings_helper_.SetBoolean(chromeos::kPluginVmAllowed, true);
    EXPECT_FALSE(IsPluginVmAllowedForProfile(testing_profile_.get()));

    settings_helper_.SetString(chromeos::kPluginVmLicenseKey, "LICENSE_KEY");
    EXPECT_FALSE(IsPluginVmAllowedForProfile(testing_profile_.get()));

    testing_profile_->GetPrefs()->Set(plugin_vm::prefs::kPluginVmImage,
                                      *base::JSONReader::ReadDeprecated(R"(
      {
          "url": "https://example.com/plugin_vm_image",
          "hash": "842841a4c75a55ad050d686f4ea5f77e83ae059877fe9b6946aa63d3d057ed32"
      }
    )"));
  }

  void SetUserRequirementsToAllowPluginVm() {
    // User for the profile should be affiliated with the device.
    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        testing_profile_->GetProfileUserName(), "id"));
    user_manager_.AddUserWithAffiliationAndType(
        account_id, true, user_manager::USER_TYPE_REGULAR);
    chromeos::ProfileHelper::Get()->SetProfileToUserMappingForTesting(
        user_manager_.GetActiveUser());
  }

  void AllowPluginVm() {
    SetPolicyRequirementsToAllowPluginVm();
    EXPECT_FALSE(IsPluginVmAllowedForProfile(testing_profile_.get()));
    SetUserRequirementsToAllowPluginVm();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PluginVmUtilTest);
};

TEST_F(PluginVmUtilTest,
       IsPluginVmAllowedForProfileReturnsTrueOnceAllConditionsAreMet) {
  EXPECT_FALSE(IsPluginVmAllowedForProfile(testing_profile_.get()));

  AllowPluginVm();

  EXPECT_TRUE(IsPluginVmAllowedForProfile(testing_profile_.get()));
}

TEST_F(PluginVmUtilTest,
       IsPluginVmConfiguredReturnsTrueOnceAllConditionsAreMet) {
  EXPECT_FALSE(IsPluginVmConfigured(testing_profile_.get()));

  testing_profile_->GetPrefs()->SetBoolean(
      plugin_vm::prefs::kPluginVmImageExists, true);

  EXPECT_TRUE(IsPluginVmConfigured(testing_profile_.get()));
}

TEST_F(PluginVmUtilTest, GetPluginVmLicenseKey) {
  // If no license key is set, the method should return the empty string.
  EXPECT_EQ(std::string(), GetPluginVmLicenseKey());

  const std::string kLicenseKey = "LICENSE_KEY";
  settings_helper_.SetString(chromeos::kPluginVmLicenseKey, kLicenseKey);
  EXPECT_EQ(kLicenseKey, GetPluginVmLicenseKey());
}

}  // namespace plugin_vm
