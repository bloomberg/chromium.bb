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

namespace {
const char kDiskImageImportCommandUuid[] = "3922722bd7394acf85bf4d5a330d4a47";
}  // namespace

void SetupConciergeForSuccessfulDiskImageImport(
    chromeos::FakeConciergeClient* fake_concierge_client_) {
  // Set immediate response for the ImportDiskImage call: will be that "image is
  // in progress":
  vm_tools::concierge::ImportDiskImageResponse import_disk_image_response;
  import_disk_image_response.set_status(
      vm_tools::concierge::DISK_STATUS_IN_PROGRESS);
  import_disk_image_response.set_command_uuid(kDiskImageImportCommandUuid);
  fake_concierge_client_->set_import_disk_image_response(
      import_disk_image_response);

  // Set a series of signals: one at 50% (in progress) and one at 100%
  // (created):
  std::vector<vm_tools::concierge::DiskImageStatusResponse> signals;
  vm_tools::concierge::DiskImageStatusResponse signal1;
  signal1.set_status(vm_tools::concierge::DISK_STATUS_IN_PROGRESS);
  signal1.set_progress(50);
  signal1.set_command_uuid(kDiskImageImportCommandUuid);
  vm_tools::concierge::DiskImageStatusResponse signal2;
  signal2.set_status(vm_tools::concierge::DISK_STATUS_CREATED);
  signal2.set_progress(100);
  signal2.set_command_uuid(kDiskImageImportCommandUuid);
  fake_concierge_client_->set_disk_image_status_signals({signal1, signal2});

  // Finally, set a success response for any eventual final call to
  // DiskImageStatus:
  vm_tools::concierge::DiskImageStatusResponse disk_image_status_response;
  disk_image_status_response.set_status(
      vm_tools::concierge::DISK_STATUS_CREATED);
  disk_image_status_response.set_command_uuid(kDiskImageImportCommandUuid);
  fake_concierge_client_->set_disk_image_status_response(
      disk_image_status_response);
}

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
