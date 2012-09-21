// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_settings_provider.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/browser/chromeos/settings/mock_owner_key_util.h"
#include "chrome/browser/policy/policy_builder.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace chromeos {

using ::testing::AnyNumber;
using ::testing::Mock;
using ::testing::_;

class DeviceSettingsProviderTest: public testing::Test {
 public:
  MOCK_METHOD1(SettingChanged, void(const std::string&));
  MOCK_METHOD0(GetTrustedCallback, void(void));

 protected:
  DeviceSettingsProviderTest()
      : message_loop_(MessageLoop::TYPE_UI),
        ui_thread_(content::BrowserThread::UI, &message_loop_),
        file_thread_(content::BrowserThread::FILE, &message_loop_),
        local_state_(static_cast<TestingBrowserProcess*>(g_browser_process)),
        owner_key_util_(new MockOwnerKeyUtil()) {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(PathService::Get(chrome::DIR_USER_DATA,
                                 &original_user_data_dir_));
    ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(PathService::Override(chrome::DIR_USER_DATA,
                                      user_data_dir_.path()));
    policy_.payload().mutable_metrics_enabled()->set_metrics_enabled(false);
    policy_.Build();

    device_settings_test_helper_.set_policy_blob(policy_.GetBlob());

    device_settings_service_.Initialize(&device_settings_test_helper_,
                                        owner_key_util_);

    EXPECT_CALL(*this, SettingChanged(_)).Times(AnyNumber());
    provider_.reset(
        new DeviceSettingsProvider(
            base::Bind(&DeviceSettingsProviderTest::SettingChanged,
                       base::Unretained(this)),
            &device_settings_service_));
    Mock::VerifyAndClearExpectations(this);
  }

  virtual void TearDown() OVERRIDE {
    device_settings_service_.Shutdown();
    ASSERT_TRUE(PathService::Override(chrome::DIR_USER_DATA,
                                      original_user_data_dir_));
  }

  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  ScopedStubCrosEnabler stub_cros_enabler_;

  ScopedTestingLocalState local_state_;

  DeviceSettingsTestHelper device_settings_test_helper_;
  scoped_refptr<MockOwnerKeyUtil> owner_key_util_;

  DeviceSettingsService device_settings_service_;

  policy::DevicePolicyBuilder policy_;

  scoped_ptr<DeviceSettingsProvider> provider_;

  ScopedTempDir user_data_dir_;
  FilePath original_user_data_dir_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceSettingsProviderTest);
};

TEST_F(DeviceSettingsProviderTest, InitializationTest) {
  owner_key_util_->SetPublicKeyFromPrivateKey(policy_.signing_key());

  // Have the service load a settings blob.
  EXPECT_CALL(*this, SettingChanged(_)).Times(AnyNumber());
  device_settings_service_.Load();
  device_settings_test_helper_.Flush();
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
  device_settings_service_.Load();
  device_settings_test_helper_.Flush();

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
  device_settings_test_helper_.Flush();
  EXPECT_EQ(std::string(), device_settings_test_helper_.policy_blob());

  // Verify the change has been applied.
  const base::Value* saved_value = provider_->Get(kReleaseChannel);
  ASSERT_TRUE(saved_value);
  EXPECT_TRUE(saved_value->GetAsString(&string_value));
  ASSERT_EQ("stable-channel", string_value);
}

TEST_F(DeviceSettingsProviderTest, SetPrefFailed) {
  // If we are not the owner no sets should work.
  owner_key_util_->SetPublicKeyFromPrivateKey(policy_.signing_key());

  base::FundamentalValue value(true);
  EXPECT_CALL(*this, SettingChanged(kStatsReportingPref)).Times(1);
  provider_->Set(kStatsReportingPref, value);
  Mock::VerifyAndClearExpectations(this);

  // This shouldn't trigger a write.
  device_settings_test_helper_.set_policy_blob(std::string());
  device_settings_test_helper_.Flush();
  EXPECT_EQ(std::string(), device_settings_test_helper_.policy_blob());

  // Verify the change has not been applied.
  const base::Value* saved_value = provider_->Get(kStatsReportingPref);
  ASSERT_TRUE(saved_value);
  bool bool_value;
  EXPECT_TRUE(saved_value->GetAsBoolean(&bool_value));
  EXPECT_FALSE(bool_value);
}

TEST_F(DeviceSettingsProviderTest, SetPrefSucceed) {
  owner_key_util_->SetPrivateKey(policy_.signing_key());
  device_settings_service_.SetUsername(policy_.policy_data().username());
  device_settings_test_helper_.Flush();

  base::FundamentalValue value(true);
  EXPECT_CALL(*this, SettingChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*this, SettingChanged(kStatsReportingPref)).Times(1);
  provider_->Set(kStatsReportingPref, value);
  Mock::VerifyAndClearExpectations(this);

  // Process the store.
  device_settings_test_helper_.set_policy_blob(std::string());
  device_settings_test_helper_.Flush();

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
  owner_key_util_->SetPrivateKey(policy_.signing_key());
  device_settings_service_.SetUsername(policy_.policy_data().username());
  device_settings_test_helper_.Flush();

  EXPECT_CALL(*this, SettingChanged(_)).Times(AnyNumber());

  base::StringValue value1("beta");
  provider_->Set(kReleaseChannel, value1);
  base::StringValue value2("dev");
  provider_->Set(kReleaseChannel, value2);

  // Let the changes propagate through the system.
  device_settings_test_helper_.set_policy_blob(std::string());
  device_settings_test_helper_.Flush();

  // Verify the second change has been applied.
  const base::Value* saved_value = provider_->Get(kReleaseChannel);
  EXPECT_TRUE(value2.Equals(saved_value));

  Mock::VerifyAndClearExpectations(this);
}

TEST_F(DeviceSettingsProviderTest, PolicyRetrievalFailedBadSignature) {
  owner_key_util_->SetPublicKeyFromPrivateKey(policy_.signing_key());
  policy_.policy().set_policy_data_signature("bad signature");
  device_settings_test_helper_.set_policy_blob(policy_.GetBlob());

  device_settings_service_.Load();
  device_settings_test_helper_.Flush();

  // Verify that the cached settings blob is not "trusted".
  EXPECT_EQ(DeviceSettingsService::STORE_VALIDATION_ERROR,
            device_settings_service_.status());
  EXPECT_EQ(CrosSettingsProvider::PERMANENTLY_UNTRUSTED,
            provider_->PrepareTrustedValues(base::Closure()));
}

TEST_F(DeviceSettingsProviderTest, PolicyRetrievalNoPolicy) {
  owner_key_util_->SetPublicKeyFromPrivateKey(policy_.signing_key());
  device_settings_test_helper_.set_policy_blob(std::string());

  device_settings_service_.Load();
  device_settings_test_helper_.Flush();

  // Verify that the cached settings blob is not "trusted".
  EXPECT_EQ(DeviceSettingsService::STORE_NO_POLICY,
            device_settings_service_.status());
  EXPECT_EQ(CrosSettingsProvider::PERMANENTLY_UNTRUSTED,
            provider_->PrepareTrustedValues(base::Closure()));
}

TEST_F(DeviceSettingsProviderTest, PolicyFailedPermanentlyNotification) {
  owner_key_util_->SetPublicKeyFromPrivateKey(policy_.signing_key());
  device_settings_test_helper_.set_policy_blob(std::string());

  EXPECT_CALL(*this, GetTrustedCallback());
  EXPECT_EQ(CrosSettingsProvider::TEMPORARILY_UNTRUSTED,
            provider_->PrepareTrustedValues(
                base::Bind(&DeviceSettingsProviderTest::GetTrustedCallback,
                           base::Unretained(this))));

  device_settings_service_.Load();
  device_settings_test_helper_.Flush();
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

  device_settings_service_.Load();
  device_settings_test_helper_.Flush();
  Mock::VerifyAndClearExpectations(this);
}

TEST_F(DeviceSettingsProviderTest, StatsReportingMigration) {
  // Create the legacy consent file.
  FilePath consent_file =
      user_data_dir_.path().AppendASCII("Consent To Send Stats");
  ASSERT_EQ(1, file_util::WriteFile(consent_file, "0", 1));

  // This should trigger migration because the metrics policy isn't in the blob.
  device_settings_test_helper_.set_policy_blob(std::string());
  device_settings_test_helper_.Flush();
  EXPECT_EQ(std::string(), device_settings_test_helper_.policy_blob());

  // Verify that migration has kicked in.
  const base::Value* saved_value = provider_->Get(kStatsReportingPref);
  ASSERT_TRUE(saved_value);
  bool bool_value;
  EXPECT_TRUE(saved_value->GetAsBoolean(&bool_value));
  EXPECT_FALSE(bool_value);
}

} // namespace chromeos
