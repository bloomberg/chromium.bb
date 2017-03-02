// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_settings_provider.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/user.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace chromeos {

using ::testing::AtLeast;
using ::testing::AnyNumber;
using ::testing::Mock;
using ::testing::_;

namespace {

const char kDisabledMessage[] = "This device has been disabled.";

}  // namespace

class DeviceSettingsProviderTest : public DeviceSettingsTestBase {
 public:
  MOCK_METHOD1(SettingChanged, void(const std::string&));
  MOCK_METHOD0(GetTrustedCallback, void(void));

 protected:
  DeviceSettingsProviderTest()
      : local_state_(TestingBrowserProcess::GetGlobal()),
        user_data_dir_override_(chrome::DIR_USER_DATA) {}

  void SetUp() override {
    DeviceSettingsTestBase::SetUp();

    EXPECT_CALL(*this, SettingChanged(_)).Times(AnyNumber());
    provider_.reset(
        new DeviceSettingsProvider(
            base::Bind(&DeviceSettingsProviderTest::SettingChanged,
                       base::Unretained(this)),
            &device_settings_service_));
    Mock::VerifyAndClearExpectations(this);
  }

  void TearDown() override { DeviceSettingsTestBase::TearDown(); }

  // Helper routine to enable/disable all reporting settings in policy.
  void SetReportingSettings(bool enable_reporting, int frequency) {
    EXPECT_CALL(*this, SettingChanged(_)).Times(AtLeast(1));
    em::DeviceReportingProto* proto =
        device_policy_.payload().mutable_device_reporting();
    proto->set_report_version_info(enable_reporting);
    proto->set_report_activity_times(enable_reporting);
    proto->set_report_boot_mode(enable_reporting);
    proto->set_report_location(enable_reporting);
    proto->set_report_network_interfaces(enable_reporting);
    proto->set_report_users(enable_reporting);
    proto->set_report_hardware_status(enable_reporting);
    proto->set_report_session_status(enable_reporting);
    proto->set_report_os_update_status(enable_reporting);
    proto->set_report_running_kiosk_app(enable_reporting);
    proto->set_device_status_frequency(frequency);
    device_policy_.Build();
    device_settings_test_helper_.set_policy_blob(device_policy_.GetBlob());
    ReloadDeviceSettings();
    Mock::VerifyAndClearExpectations(this);
  }

  // Helper routine to enable/disable all reporting settings in policy.
  void SetHeartbeatSettings(bool enable_heartbeat, int frequency) {
    EXPECT_CALL(*this, SettingChanged(_)).Times(AtLeast(1));
    em::DeviceHeartbeatSettingsProto* proto =
        device_policy_.payload().mutable_device_heartbeat_settings();
    proto->set_heartbeat_enabled(enable_heartbeat);
    proto->set_heartbeat_frequency(frequency);
    device_policy_.Build();
    device_settings_test_helper_.set_policy_blob(device_policy_.GetBlob());
    ReloadDeviceSettings();
    Mock::VerifyAndClearExpectations(this);
  }

  // Helper routine to enable/disable log upload settings in policy.
  void SetLogUploadSettings(bool enable_system_log_upload) {
    EXPECT_CALL(*this, SettingChanged(_)).Times(AtLeast(1));
    em::DeviceLogUploadSettingsProto* proto =
        device_policy_.payload().mutable_device_log_upload_settings();
    proto->set_system_log_upload_enabled(enable_system_log_upload);
    device_policy_.Build();
    device_settings_test_helper_.set_policy_blob(device_policy_.GetBlob());
    ReloadDeviceSettings();
    Mock::VerifyAndClearExpectations(this);
  }

  // Helper routine to enable/disable metrics report upload settings in policy.
  void SetMetricsReportingSettings(bool enable_metrics_reporting) {
    EXPECT_CALL(*this, SettingChanged(_)).Times(AtLeast(1));
    em::MetricsEnabledProto* proto =
        device_policy_.payload().mutable_metrics_enabled();
    proto->set_metrics_enabled(enable_metrics_reporting);
    device_policy_.Build();
    device_settings_test_helper_.set_policy_blob(device_policy_.GetBlob());
    ReloadDeviceSettings();
    Mock::VerifyAndClearExpectations(this);
  }

  // Helper routine to ensure all heartbeat policies have been correctly
  // decoded.
  void VerifyHeartbeatSettings(bool expected_enable_state,
                               int expected_frequency) {
    const base::Value expected_enabled_value(expected_enable_state);
    EXPECT_TRUE(base::Value::Equals(provider_->Get(kHeartbeatEnabled),
                                    &expected_enabled_value));

    const base::Value expected_frequency_value(expected_frequency);
    EXPECT_TRUE(base::Value::Equals(provider_->Get(kHeartbeatFrequency),
                                    &expected_frequency_value));
  }

  // Helper routine to ensure all reporting policies have been correctly
  // decoded.
  void VerifyReportingSettings(bool expected_enable_state,
                               int expected_frequency) {
    const char* reporting_settings[] = {
      kReportDeviceVersionInfo,
      kReportDeviceActivityTimes,
      kReportDeviceBootMode,
      // Device location reporting is not currently supported.
      // kReportDeviceLocation,
      kReportDeviceNetworkInterfaces,
      kReportDeviceUsers,
      kReportDeviceHardwareStatus,
      kReportDeviceSessionStatus,
      kReportOsUpdateStatus,
      kReportRunningKioskApp
    };

    const base::Value expected_enable_value(expected_enable_state);
    for (auto* setting : reporting_settings) {
      EXPECT_TRUE(base::Value::Equals(provider_->Get(setting),
                                      &expected_enable_value))
          << "Value for " << setting << " does not match expected";
    }
    const base::Value expected_frequency_value(expected_frequency);
    EXPECT_TRUE(base::Value::Equals(provider_->Get(kReportUploadFrequency),
                                    &expected_frequency_value));
  }

  // Helper routine to ensure log upload policy has been correctly
  // decoded.
  void VerifyLogUploadSettings(bool expected_enable_state) {
    const base::Value expected_enabled_value(expected_enable_state);
    EXPECT_TRUE(base::Value::Equals(provider_->Get(kSystemLogUploadEnabled),
                                    &expected_enabled_value));
  }

  // Helper routine to set LoginScreenDomainAutoComplete policy.
  void SetDomainAutoComplete(const std::string& domain) {
    EXPECT_CALL(*this, SettingChanged(_)).Times(AtLeast(1));
    em::LoginScreenDomainAutoCompleteProto* proto =
        device_policy_.payload().mutable_login_screen_domain_auto_complete();
    proto->set_login_screen_domain_auto_complete(domain);
    device_policy_.Build();
    device_settings_test_helper_.set_policy_blob(device_policy_.GetBlob());
    ReloadDeviceSettings();
    Mock::VerifyAndClearExpectations(this);
  }

  // Helper routine to check value of the LoginScreenDomainAutoComplete policy.
  void VerifyDomainAutoComplete(
      const base::StringValue* const ptr_to_expected_value) {
    EXPECT_TRUE(base::Value::Equals(
        provider_->Get(kAccountsPrefLoginScreenDomainAutoComplete),
        ptr_to_expected_value));
  }

  ScopedTestingLocalState local_state_;

  std::unique_ptr<DeviceSettingsProvider> provider_;

  base::ScopedPathOverride user_data_dir_override_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceSettingsProviderTest);
};

TEST_F(DeviceSettingsProviderTest, InitializationTest) {
  // Have the service load a settings blob.
  EXPECT_CALL(*this, SettingChanged(_)).Times(AnyNumber());
  ReloadDeviceSettings();
  Mock::VerifyAndClearExpectations(this);

  // Verify that the policy blob has been correctly parsed and trusted.
  // The trusted flag should be set before the call to PrepareTrustedValues.
  EXPECT_EQ(CrosSettingsProvider::TRUSTED,
            provider_->PrepareTrustedValues(base::Closure()));
  const base::Value* value = provider_->Get(kStatsReportingPref);
  ASSERT_TRUE(value);
  bool bool_value;
  EXPECT_TRUE(value->GetAsBoolean(&bool_value));
  EXPECT_FALSE(bool_value);
}

TEST_F(DeviceSettingsProviderTest, InitializationTestUnowned) {
  // Have the service check the key.
  owner_key_util_->Clear();
  ReloadDeviceSettings();

  // The trusted flag should be set before the call to PrepareTrustedValues.
  EXPECT_EQ(CrosSettingsProvider::TRUSTED,
            provider_->PrepareTrustedValues(base::Closure()));
  const base::Value* value = provider_->Get(kReleaseChannel);
  ASSERT_TRUE(value);
  std::string string_value;
  EXPECT_TRUE(value->GetAsString(&string_value));
  EXPECT_TRUE(string_value.empty());

  // Sets should succeed though and be readable from the cache.
  EXPECT_CALL(*this, SettingChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*this, SettingChanged(kReleaseChannel)).Times(1);
  base::StringValue new_value("stable-channel");
  provider_->Set(kReleaseChannel, new_value);
  Mock::VerifyAndClearExpectations(this);

  // This shouldn't trigger a write.
  device_settings_test_helper_.set_policy_blob(std::string());
  FlushDeviceSettings();
  EXPECT_EQ(std::string(), device_settings_test_helper_.policy_blob());

  // Verify the change has been applied.
  const base::Value* saved_value = provider_->Get(kReleaseChannel);
  ASSERT_TRUE(saved_value);
  EXPECT_TRUE(saved_value->GetAsString(&string_value));
  ASSERT_EQ("stable-channel", string_value);
}

TEST_F(DeviceSettingsProviderTest, SetPrefFailed) {
  SetMetricsReportingSettings(false);

  // If we are not the owner no sets should work.
  base::Value value(true);
  EXPECT_CALL(*this, SettingChanged(kStatsReportingPref)).Times(1);
  provider_->Set(kStatsReportingPref, value);
  Mock::VerifyAndClearExpectations(this);

  // This shouldn't trigger a write.
  device_settings_test_helper_.set_policy_blob(std::string());
  FlushDeviceSettings();
  EXPECT_EQ(std::string(), device_settings_test_helper_.policy_blob());

  // Verify the change has not been applied.
  const base::Value* saved_value = provider_->Get(kStatsReportingPref);
  ASSERT_TRUE(saved_value);
  bool bool_value;
  EXPECT_TRUE(saved_value->GetAsBoolean(&bool_value));
  EXPECT_FALSE(bool_value);
}

TEST_F(DeviceSettingsProviderTest, SetPrefSucceed) {
  owner_key_util_->SetPrivateKey(device_policy_.GetSigningKey());
  InitOwner(AccountId::FromUserEmail(device_policy_.policy_data().username()),
            true);
  FlushDeviceSettings();

  base::Value value(true);
  EXPECT_CALL(*this, SettingChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*this, SettingChanged(kStatsReportingPref)).Times(1);
  provider_->Set(kStatsReportingPref, value);
  Mock::VerifyAndClearExpectations(this);

  // Process the store.
  device_settings_test_helper_.set_policy_blob(std::string());
  FlushDeviceSettings();

  // Verify that the device policy has been adjusted.
  ASSERT_TRUE(device_settings_service_.device_settings());
  EXPECT_TRUE(device_settings_service_.device_settings()->
                  metrics_enabled().metrics_enabled());

  // Verify the change has been applied.
  const base::Value* saved_value = provider_->Get(kStatsReportingPref);
  ASSERT_TRUE(saved_value);
  bool bool_value;
  EXPECT_TRUE(saved_value->GetAsBoolean(&bool_value));
  EXPECT_TRUE(bool_value);
}

TEST_F(DeviceSettingsProviderTest, SetPrefTwice) {
  owner_key_util_->SetPrivateKey(device_policy_.GetSigningKey());
  InitOwner(AccountId::FromUserEmail(device_policy_.policy_data().username()),
            true);
  FlushDeviceSettings();

  EXPECT_CALL(*this, SettingChanged(_)).Times(AnyNumber());

  base::StringValue value1("beta");
  provider_->Set(kReleaseChannel, value1);
  base::StringValue value2("dev");
  provider_->Set(kReleaseChannel, value2);

  // Let the changes propagate through the system.
  device_settings_test_helper_.set_policy_blob(std::string());
  FlushDeviceSettings();

  // Verify the second change has been applied.
  const base::Value* saved_value = provider_->Get(kReleaseChannel);
  EXPECT_TRUE(value2.Equals(saved_value));

  Mock::VerifyAndClearExpectations(this);
}

TEST_F(DeviceSettingsProviderTest, PolicyRetrievalFailedBadSignature) {
  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_.GetSigningKey());
  device_policy_.policy().set_policy_data_signature("bad signature");
  device_settings_test_helper_.set_policy_blob(device_policy_.GetBlob());
  ReloadDeviceSettings();

  // Verify that the cached settings blob is not "trusted".
  EXPECT_EQ(DeviceSettingsService::STORE_VALIDATION_ERROR,
            device_settings_service_.status());
  EXPECT_EQ(CrosSettingsProvider::PERMANENTLY_UNTRUSTED,
            provider_->PrepareTrustedValues(base::Closure()));
}

TEST_F(DeviceSettingsProviderTest, PolicyRetrievalNoPolicy) {
  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_.GetSigningKey());
  device_settings_test_helper_.set_policy_blob(std::string());
  ReloadDeviceSettings();

  // Verify that the cached settings blob is not "trusted".
  EXPECT_EQ(DeviceSettingsService::STORE_NO_POLICY,
            device_settings_service_.status());
  EXPECT_EQ(CrosSettingsProvider::PERMANENTLY_UNTRUSTED,
            provider_->PrepareTrustedValues(base::Closure()));
}

TEST_F(DeviceSettingsProviderTest, PolicyFailedPermanentlyNotification) {
  device_settings_test_helper_.set_policy_blob(std::string());

  EXPECT_CALL(*this, GetTrustedCallback());
  EXPECT_EQ(CrosSettingsProvider::TEMPORARILY_UNTRUSTED,
            provider_->PrepareTrustedValues(
                base::Bind(&DeviceSettingsProviderTest::GetTrustedCallback,
                           base::Unretained(this))));

  ReloadDeviceSettings();
  Mock::VerifyAndClearExpectations(this);

  EXPECT_EQ(CrosSettingsProvider::PERMANENTLY_UNTRUSTED,
            provider_->PrepareTrustedValues(base::Closure()));
}

TEST_F(DeviceSettingsProviderTest, PolicyLoadNotification) {
  EXPECT_CALL(*this, GetTrustedCallback());

  EXPECT_EQ(CrosSettingsProvider::TEMPORARILY_UNTRUSTED,
            provider_->PrepareTrustedValues(
                base::Bind(&DeviceSettingsProviderTest::GetTrustedCallback,
                           base::Unretained(this))));

  ReloadDeviceSettings();
  Mock::VerifyAndClearExpectations(this);
}

TEST_F(DeviceSettingsProviderTest, LegacyDeviceLocalAccounts) {
  EXPECT_CALL(*this, SettingChanged(_)).Times(AnyNumber());
  em::DeviceLocalAccountInfoProto* account =
      device_policy_.payload().mutable_device_local_accounts()->add_account();
  account->set_deprecated_public_session_id(
      policy::PolicyBuilder::kFakeUsername);
  device_policy_.Build();
  device_settings_test_helper_.set_policy_blob(device_policy_.GetBlob());
  ReloadDeviceSettings();
  Mock::VerifyAndClearExpectations(this);

  // On load, the deprecated spec should have been converted to the new format.
  base::ListValue expected_accounts;
  std::unique_ptr<base::DictionaryValue> entry_dict(
      new base::DictionaryValue());
  entry_dict->SetString(kAccountsPrefDeviceLocalAccountsKeyId,
                        policy::PolicyBuilder::kFakeUsername);
  entry_dict->SetInteger(kAccountsPrefDeviceLocalAccountsKeyType,
                         policy::DeviceLocalAccount::TYPE_PUBLIC_SESSION);
  expected_accounts.Append(std::move(entry_dict));
  const base::Value* actual_accounts =
      provider_->Get(kAccountsPrefDeviceLocalAccounts);
  EXPECT_TRUE(base::Value::Equals(&expected_accounts, actual_accounts));
}

TEST_F(DeviceSettingsProviderTest, DecodeDeviceState) {
  EXPECT_CALL(*this, SettingChanged(_)).Times(AtLeast(1));
  device_policy_.policy_data().mutable_device_state()->set_device_mode(
      em::DeviceState::DEVICE_MODE_DISABLED);
  device_policy_.policy_data().mutable_device_state()->
      mutable_disabled_state()->set_message(kDisabledMessage);
  device_policy_.Build();
  device_settings_test_helper_.set_policy_blob(device_policy_.GetBlob());
  ReloadDeviceSettings();
  Mock::VerifyAndClearExpectations(this);

  // Verify that the device state has been decoded correctly.
  const base::Value expected_disabled_value(true);
  EXPECT_TRUE(base::Value::Equals(provider_->Get(kDeviceDisabled),
                                  &expected_disabled_value));
  const base::StringValue expected_disabled_message_value(kDisabledMessage);
  EXPECT_TRUE(base::Value::Equals(provider_->Get(kDeviceDisabledMessage),
                                  &expected_disabled_message_value));

  // Verify that a change to the device state triggers a notification.
  EXPECT_CALL(*this, SettingChanged(_)).Times(AtLeast(1));
  device_policy_.policy_data().mutable_device_state()->clear_device_mode();
  device_policy_.Build();
  device_settings_test_helper_.set_policy_blob(device_policy_.GetBlob());
  ReloadDeviceSettings();
  Mock::VerifyAndClearExpectations(this);

  // Verify that the updated state has been decoded correctly.
  EXPECT_FALSE(provider_->Get(kDeviceDisabled));
}

TEST_F(DeviceSettingsProviderTest, DecodeReportingSettings) {
  // Turn on all reporting and verify that the reporting settings have been
  // decoded correctly.
  const int status_frequency = 50000;
  SetReportingSettings(true, status_frequency);
  VerifyReportingSettings(true, status_frequency);

  // Turn off all reporting and verify that the settings are decoded
  // correctly.
  SetReportingSettings(false, status_frequency);
  VerifyReportingSettings(false, status_frequency);
}

TEST_F(DeviceSettingsProviderTest, DecodeHeartbeatSettings) {
  // Turn on heartbeats and verify that the heartbeat settings have been
  // decoded correctly.
  const int heartbeat_frequency = 50000;
  SetHeartbeatSettings(true, heartbeat_frequency);
  VerifyHeartbeatSettings(true, heartbeat_frequency);

  // Turn off all reporting and verify that the settings are decoded
  // correctly.
  SetHeartbeatSettings(false, heartbeat_frequency);
  VerifyHeartbeatSettings(false, heartbeat_frequency);
}

TEST_F(DeviceSettingsProviderTest, DecodeDomainAutoComplete) {
  // By default LoginScreenDomainAutoComplete policy should not be set.
  VerifyDomainAutoComplete(nullptr);

  // Empty string means that the policy is not set.
  SetDomainAutoComplete("");
  VerifyDomainAutoComplete(nullptr);

  // Check some meaningful value. Policy should be set.
  const std::string domain = "domain.test";
  const base::StringValue domain_value(domain);
  SetDomainAutoComplete(domain);
  VerifyDomainAutoComplete(&domain_value);
}

TEST_F(DeviceSettingsProviderTest, DecodeLogUploadSettings) {
  SetLogUploadSettings(true);
  VerifyLogUploadSettings(true);

  SetLogUploadSettings(false);
  VerifyLogUploadSettings(false);
}
} // namespace chromeos
