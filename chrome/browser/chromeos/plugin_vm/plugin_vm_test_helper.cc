// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_test_helper.h"

#include "base/json/json_reader.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plugin_vm {

PluginVmTestHelper::PluginVmTestHelper(TestingProfile* testing_profile)
    : testing_profile_(testing_profile) {
  testing_profile_->ScopedCrosSettingsTestHelper()
      ->ReplaceDeviceSettingsProviderWithStub();
}

PluginVmTestHelper::~PluginVmTestHelper() = default;

void PluginVmTestHelper::SetPolicyRequirementsToAllowPluginVm() {
  testing_profile_->ScopedCrosSettingsTestHelper()->SetBoolean(
      chromeos::kPluginVmAllowed, true);
  testing_profile_->ScopedCrosSettingsTestHelper()->SetString(
      chromeos::kPluginVmLicenseKey, "LICENSE_KEY");

  testing_profile_->GetPrefs()->Set(plugin_vm::prefs::kPluginVmImage,
                                    *base::JSONReader::ReadDeprecated(R"(
    {
        "url": "https://example.com/plugin_vm_image",
        "hash": "842841a4c75a55ad050d686f4ea5f77e83ae059877fe9b6946aa63d3d057ed32"
    }
  )"));
}

void PluginVmTestHelper::SetUserRequirementsToAllowPluginVm() {
  // User for the profile should be affiliated with the device.
  const AccountId account_id(AccountId::FromUserEmailGaiaId(
      testing_profile_->GetProfileUserName(), "id"));
  user_manager_.AddUserWithAffiliationAndType(account_id, true,
                                              user_manager::USER_TYPE_REGULAR);
  chromeos::ProfileHelper::Get()->SetProfileToUserMappingForTesting(
      user_manager_.GetActiveUser());
}

void PluginVmTestHelper::AllowPluginVm() {
  ASSERT_FALSE(IsPluginVmAllowedForProfile(testing_profile_));
  SetPolicyRequirementsToAllowPluginVm();
  SetUserRequirementsToAllowPluginVm();
  ASSERT_TRUE(IsPluginVmAllowedForProfile(testing_profile_));
}

}  // namespace plugin_vm
