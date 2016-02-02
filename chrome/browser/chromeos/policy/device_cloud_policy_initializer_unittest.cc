// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_cloud_policy_initializer.h"

#include "base/values.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "chrome/browser/chromeos/policy/server_backed_device_state.h"
#include "chrome/browser/chromeos/policy/stub_enterprise_install_attributes.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/system/statistics_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class DeviceCloudPolicyInitializerTest : public testing::Test {
 protected:
  DeviceCloudPolicyInitializerTest()
      : device_cloud_policy_initializer_(
            &local_state_,
            nullptr,
            nullptr,
            nullptr,
            &install_attributes_,
            nullptr,
            nullptr,
            nullptr) {
    chrome::RegisterLocalState(local_state_.registry());
    statistics_provider_.SetMachineStatistic("serial_number", "fake-serial");
  }

  chromeos::system::ScopedFakeStatisticsProvider statistics_provider_;
  TestingPrefServiceSimple local_state_;
  StubEnterpriseInstallAttributes install_attributes_;
  DeviceCloudPolicyInitializer device_cloud_policy_initializer_;
};

TEST_F(DeviceCloudPolicyInitializerTest,
       GetPrescribedEnrollmentConfigDuringOOBE) {
  // Default configuration is empty.
  EnrollmentConfig config =
      device_cloud_policy_initializer_.GetPrescribedEnrollmentConfig();
  EXPECT_EQ(EnrollmentConfig::MODE_NONE, config.mode);
  EXPECT_TRUE(config.management_domain.empty());

  // Set signals in increasing order of precedence, check results.

  // OEM manifest: advertised enrollment.
  statistics_provider_.SetMachineFlag(
      chromeos::system::kOemIsEnterpriseManagedKey, true);
  config = device_cloud_policy_initializer_.GetPrescribedEnrollmentConfig();
  EXPECT_EQ(EnrollmentConfig::MODE_LOCAL_ADVERTISED, config.mode);
  EXPECT_TRUE(config.management_domain.empty());

  // Pref: advertised enrollment. The resulting |config| is indistinguishable
  // from the OEM manifest configuration, so clear the latter to at least verify
  // the pref configuration results in the expect behavior on its own.
  statistics_provider_.ClearMachineFlag(
      chromeos::system::kOemIsEnterpriseManagedKey);
  local_state_.SetBoolean(prefs::kDeviceEnrollmentAutoStart, true);
  config = device_cloud_policy_initializer_.GetPrescribedEnrollmentConfig();
  EXPECT_EQ(EnrollmentConfig::MODE_LOCAL_ADVERTISED, config.mode);
  EXPECT_TRUE(config.management_domain.empty());

  // Server-backed state: advertised enrollment.
  base::DictionaryValue state_dict;
  state_dict.SetString(kDeviceStateRestoreMode,
                       kDeviceStateRestoreModeReEnrollmentRequested);
  state_dict.SetString(kDeviceStateManagementDomain, "example.com");
  local_state_.Set(prefs::kServerBackedDeviceState, state_dict);
  config = device_cloud_policy_initializer_.GetPrescribedEnrollmentConfig();
  EXPECT_EQ(EnrollmentConfig::MODE_SERVER_ADVERTISED, config.mode);
  EXPECT_EQ("example.com", config.management_domain);

  // OEM manifest: forced enrollment.
  statistics_provider_.SetMachineFlag(
      chromeos::system::kOemIsEnterpriseManagedKey, true);
  statistics_provider_.SetMachineFlag(
      chromeos::system::kOemCanExitEnterpriseEnrollmentKey, false);
  config = device_cloud_policy_initializer_.GetPrescribedEnrollmentConfig();
  EXPECT_EQ(EnrollmentConfig::MODE_LOCAL_FORCED, config.mode);
  EXPECT_TRUE(config.management_domain.empty());

  // Pref: forced enrollment. The resulting |config| is indistinguishable from
  // the OEM manifest configuration, so clear the latter to at least verify the
  // pref configuration results in the expect behavior on its own.
  statistics_provider_.ClearMachineFlag(
      chromeos::system::kOemIsEnterpriseManagedKey);
  local_state_.SetBoolean(prefs::kDeviceEnrollmentCanExit, false);
  config = device_cloud_policy_initializer_.GetPrescribedEnrollmentConfig();
  EXPECT_EQ(EnrollmentConfig::MODE_LOCAL_FORCED, config.mode);
  EXPECT_TRUE(config.management_domain.empty());

  // Server-backed state: forced enrollment.
  state_dict.SetString(kDeviceStateRestoreMode,
                       kDeviceStateRestoreModeReEnrollmentEnforced);
  local_state_.Set(prefs::kServerBackedDeviceState, state_dict);
  config = device_cloud_policy_initializer_.GetPrescribedEnrollmentConfig();
  EXPECT_EQ(EnrollmentConfig::MODE_SERVER_FORCED, config.mode);
  EXPECT_EQ("example.com", config.management_domain);
}

TEST_F(DeviceCloudPolicyInitializerTest,
       GetPrescribedEnrollmentConfigAfterOOBE) {
  // If OOBE is complete, we may re-enroll to the domain configured in install
  // attributes. This is only enforced after detecting enrollment loss.
  local_state_.SetBoolean(prefs::kOobeComplete, true);
  EnrollmentConfig config =
      device_cloud_policy_initializer_.GetPrescribedEnrollmentConfig();
  EXPECT_EQ(EnrollmentConfig::MODE_NONE, config.mode);
  EXPECT_TRUE(config.management_domain.empty());

  // Advertised enrollment gets ignored.
  local_state_.SetBoolean(prefs::kDeviceEnrollmentAutoStart, true);
  statistics_provider_.SetMachineFlag(
      chromeos::system::kOemIsEnterpriseManagedKey, true);
  config = device_cloud_policy_initializer_.GetPrescribedEnrollmentConfig();
  EXPECT_EQ(EnrollmentConfig::MODE_NONE, config.mode);
  EXPECT_TRUE(config.management_domain.empty());

  // If the device is enterprise-managed, the management domain gets pulled from
  // install attributes.
  install_attributes_.SetRegistrationUser("user@example.com");
  install_attributes_.SetDomain("example.com");
  config = device_cloud_policy_initializer_.GetPrescribedEnrollmentConfig();
  EXPECT_EQ(EnrollmentConfig::MODE_NONE, config.mode);
  EXPECT_EQ("example.com", config.management_domain);

  // If enrollment recovery is on, this is signaled in |config.mode|.
  local_state_.SetBoolean(prefs::kEnrollmentRecoveryRequired, true);
  config = device_cloud_policy_initializer_.GetPrescribedEnrollmentConfig();
  EXPECT_EQ(EnrollmentConfig::MODE_RECOVERY, config.mode);
  EXPECT_EQ("example.com", config.management_domain);
}

}  // namespace policy
