// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_settings_provider.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/users/fake_user_manager.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/user_manager/user.h"
#include "policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace chromeos {

using ::testing::AnyNumber;
using ::testing::Mock;
using ::testing::_;

class DeviceSettingsProviderTest : public DeviceSettingsTestBase {
 public:
  MOCK_METHOD1(SettingChanged, void(const std::string&));
  MOCK_METHOD0(GetTrustedCallback, void(void));

 protected:
  DeviceSettingsProviderTest()
      : local_state_(TestingBrowserProcess::GetGlobal()),
        user_data_dir_override_(chrome::DIR_USER_DATA) {}

  virtual void SetUp() OVERRIDE {
    DeviceSettingsTestBase::SetUp();

    EXPECT_CALL(*this, SettingChanged(_)).Times(AnyNumber());
    provider_.reset(
        new DeviceSettingsProvider(
            base::Bind(&DeviceSettingsProviderTest::SettingChanged,
                       base::Unretained(this)),
            &device_settings_service_));
    Mock::VerifyAndClearExpectations(this);
  }

  virtual void TearDown() OVERRIDE {
    DeviceSettingsTestBase::TearDown();
  }

  ScopedTestingLocalState local_state_;

  scoped_ptr<DeviceSettingsProvider> provider_;

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
  // If we are not the owner no sets should work.
  base::FundamentalValue value(true);
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
  InitOwner(device_policy_.policy_data().username(), true);
  FlushDeviceSettings();

  base::FundamentalValue value(true);
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
  InitOwner(device_policy_.policy_data().username(), true);
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

TEST_F(DeviceSettingsProviderTest, StatsReportingMigration) {
  // Create the legacy consent file.
  base::FilePath consent_file;
  ASSERT_TRUE(PathService::Get(chrome::DIR_USER_DATA, &consent_file));
  consent_file = consent_file.AppendASCII("Consent To Send Stats");
  ASSERT_EQ(1, base::WriteFile(consent_file, "0", 1));

  // This should trigger migration because the metrics policy isn't in the blob.
  device_settings_test_helper_.set_policy_blob(std::string());
  FlushDeviceSettings();
  EXPECT_EQ(std::string(), device_settings_test_helper_.policy_blob());

  // Verify that migration has kicked in.
  const base::Value* saved_value = provider_->Get(kStatsReportingPref);
  ASSERT_TRUE(saved_value);
  bool bool_value;
  EXPECT_TRUE(saved_value->GetAsBoolean(&bool_value));
  EXPECT_FALSE(bool_value);
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
  scoped_ptr<base::DictionaryValue> entry_dict(new base::DictionaryValue());
  entry_dict->SetString(kAccountsPrefDeviceLocalAccountsKeyId,
                        policy::PolicyBuilder::kFakeUsername);
  entry_dict->SetInteger(kAccountsPrefDeviceLocalAccountsKeyType,
                         policy::DeviceLocalAccount::TYPE_PUBLIC_SESSION);
  expected_accounts.Append(entry_dict.release());
  const base::Value* actual_accounts =
      provider_->Get(kAccountsPrefDeviceLocalAccounts);
  EXPECT_TRUE(base::Value::Equals(&expected_accounts, actual_accounts));
}

} // namespace chromeos
