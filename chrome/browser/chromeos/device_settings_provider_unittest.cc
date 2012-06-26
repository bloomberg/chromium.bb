// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/device_settings_provider.h"

#include <string>

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros_settings_names.h"
#include "chrome/browser/chromeos/login/mock_signed_settings_helper.h"
#include "chrome/browser/chromeos/login/mock_user_manager.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;
namespace chromeos {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;

class DeviceSettingsProviderTest: public testing::Test {
public:
  MOCK_METHOD1(SettingChanged, void(const std::string&));
  MOCK_METHOD0(GetTrustedCallback, void(void));

protected:
  DeviceSettingsProviderTest()
      : message_loop_(MessageLoop::TYPE_UI),
        ui_thread_(content::BrowserThread::UI, &message_loop_),
        file_thread_(content::BrowserThread::FILE, &message_loop_),
        local_state_(static_cast<TestingBrowserProcess*>(g_browser_process)) {
  }

  virtual ~DeviceSettingsProviderTest() {
  }

  virtual void SetUp() OVERRIDE {
    PrepareEmptyPolicy();

    EXPECT_CALL(*this, SettingChanged(_))
        .Times(AnyNumber());

    EXPECT_CALL(signed_settings_helper_, StartRetrievePolicyOp(_))
        .WillRepeatedly(
            MockSignedSettingsHelperRetrievePolicy(SignedSettings::SUCCESS,
                                                   policy_blob_));
    EXPECT_CALL(signed_settings_helper_, StartStorePolicyOp(_,_))
        .WillRepeatedly(DoAll(
            SaveArg<0>(&policy_blob_),
            MockSignedSettingsHelperStorePolicy(SignedSettings::SUCCESS)));

    EXPECT_CALL(*mock_user_manager_.user_manager(), IsCurrentUserOwner())
        .WillRepeatedly(Return(true));

    provider_.reset(
        new DeviceSettingsProvider(
            base::Bind(&DeviceSettingsProviderTest::SettingChanged,
                       base::Unretained(this)),
            &signed_settings_helper_));
    provider_->set_ownership_status(OwnershipService::OWNERSHIP_TAKEN);
    // To prevent flooding the logs.
    provider_->set_retries_left(1);
    provider_->Reload();
  }

  void PrepareEmptyPolicy() {
    em::PolicyData policy;
    em::ChromeDeviceSettingsProto pol;
    // Set metrics to disabled to prevent us from running into code that is not
    // mocked.
    pol.mutable_metrics_enabled()->set_metrics_enabled(false);
    policy.set_policy_type(chromeos::kDevicePolicyType);
    policy.set_username("me@owner");
    policy.set_policy_value(pol.SerializeAsString());
    // Wipe the signed settings store.
    policy_blob_.set_policy_data(policy.SerializeAsString());
    policy_blob_.set_policy_data_signature("false");
  }

  em::PolicyFetchResponse policy_blob_;

  scoped_ptr<DeviceSettingsProvider> provider_;

  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  ScopedTestingLocalState local_state_;

  MockSignedSettingsHelper signed_settings_helper_;

  ScopedStubCrosEnabler stub_cros_enabler_;
  ScopedMockUserManagerEnabler mock_user_manager_;
};

TEST_F(DeviceSettingsProviderTest, InitializationTest) {
  // Verify that the policy blob has been correctly parsed and trusted.
  // The trusted flag should be set before the call to PrepareTrustedValues.
  EXPECT_EQ(CrosSettingsProvider::TRUSTED,
            provider_->PrepareTrustedValues(
                base::Bind(&DeviceSettingsProviderTest::GetTrustedCallback,
                           base::Unretained(this))));
  const base::Value* value = provider_->Get(kStatsReportingPref);
  ASSERT_TRUE(value);
  bool bool_value;
  EXPECT_TRUE(value->GetAsBoolean(&bool_value));
  EXPECT_FALSE(bool_value);
}

TEST_F(DeviceSettingsProviderTest, InitializationTestUnowned) {
  // No calls to the SignedSettingsHelper should occur in this case!
  Mock::VerifyAndClear(&signed_settings_helper_);

  provider_->set_ownership_status(OwnershipService::OWNERSHIP_NONE);
  provider_->Reload();
  // The trusted flag should be set before the call to PrepareTrustedValues.
  EXPECT_EQ(CrosSettingsProvider::TRUSTED,
            provider_->PrepareTrustedValues(
                base::Bind(&DeviceSettingsProviderTest::GetTrustedCallback,
                           base::Unretained(this))));
  const base::Value* value = provider_->Get(kReleaseChannel);
  ASSERT_TRUE(value);
  std::string string_value;
  EXPECT_TRUE(value->GetAsString(&string_value));
  EXPECT_TRUE(string_value.empty());

  // Sets should succeed though and be readable from the cache.
  base::StringValue new_value("stable-channel");
  provider_->Set(kReleaseChannel, new_value);
  // Do one more reload here to make sure we don't flip randomly between stores.
  provider_->Reload();
  // Verify the change has not been applied.
  const base::Value* saved_value = provider_->Get(kReleaseChannel);
  ASSERT_TRUE(saved_value);
  EXPECT_TRUE(saved_value->GetAsString(&string_value));
  ASSERT_EQ("stable-channel", string_value);
}

TEST_F(DeviceSettingsProviderTest, SetPrefFailed) {
  // If we are not the owner no sets should work.
  EXPECT_CALL(*mock_user_manager_.user_manager(), IsCurrentUserOwner())
      .WillOnce(Return(false));
  base::FundamentalValue value(true);
  provider_->Set(kStatsReportingPref, value);
  // Verify the change has not been applied.
  const base::Value* saved_value = provider_->Get(kStatsReportingPref);
  ASSERT_TRUE(saved_value);
  bool bool_value;
  EXPECT_TRUE(saved_value->GetAsBoolean(&bool_value));
  EXPECT_FALSE(bool_value);
}

TEST_F(DeviceSettingsProviderTest, SetPrefSucceed) {
  base::FundamentalValue value(true);
  provider_->Set(kStatsReportingPref, value);
  // Verify the change has not been applied.
  const base::Value* saved_value = provider_->Get(kStatsReportingPref);
  ASSERT_TRUE(saved_value);
  bool bool_value;
  EXPECT_TRUE(saved_value->GetAsBoolean(&bool_value));
  EXPECT_TRUE(bool_value);
}

TEST_F(DeviceSettingsProviderTest, PolicyRetrievalFailedBadSingature) {
  // No calls to the SignedSettingsHelper should occur in this case!
  Mock::VerifyAndClear(&signed_settings_helper_);
  EXPECT_CALL(signed_settings_helper_, StartRetrievePolicyOp(_))
      .WillRepeatedly(
          MockSignedSettingsHelperRetrievePolicy(
              SignedSettings::BAD_SIGNATURE,
              policy_blob_));
  provider_->Reload();
  // Verify that the cache policy blob is not "trusted".
  EXPECT_EQ(CrosSettingsProvider::PERMANENTLY_UNTRUSTED,
            provider_->PrepareTrustedValues(
                base::Bind(&DeviceSettingsProviderTest::GetTrustedCallback,
                           base::Unretained(this))));
}

TEST_F(DeviceSettingsProviderTest, PolicyRetrievalOperationFailedPermanently) {
  // No calls to the SignedSettingsHelper should occur in this case!
  Mock::VerifyAndClear(&signed_settings_helper_);
  EXPECT_CALL(signed_settings_helper_, StartRetrievePolicyOp(_))
      .WillRepeatedly(
          MockSignedSettingsHelperRetrievePolicy(
              SignedSettings::OPERATION_FAILED,
              policy_blob_));
  provider_->Reload();
  // Verify that the cache policy blob is not "trusted".
  EXPECT_EQ(CrosSettingsProvider::PERMANENTLY_UNTRUSTED,
            provider_->PrepareTrustedValues(
                base::Bind(&DeviceSettingsProviderTest::GetTrustedCallback,
                           base::Unretained(this))));
}

TEST_F(DeviceSettingsProviderTest, PolicyRetrievalOperationFailedOnce) {
  // No calls to the SignedSettingsHelper should occur in this case!
  Mock::VerifyAndClear(&signed_settings_helper_);
  EXPECT_CALL(signed_settings_helper_, StartRetrievePolicyOp(_))
      .WillOnce(
          MockSignedSettingsHelperRetrievePolicy(
              SignedSettings::OPERATION_FAILED,
              policy_blob_))
      .WillRepeatedly(
          MockSignedSettingsHelperRetrievePolicy(
              SignedSettings::SUCCESS,
              policy_blob_));
  // Should be trusted after an automatic reload.
  provider_->Reload();
  // Verify that the cache policy blob is not "trusted".
  EXPECT_EQ(CrosSettingsProvider::TRUSTED,
            provider_->PrepareTrustedValues(
                base::Bind(&DeviceSettingsProviderTest::GetTrustedCallback,
                           base::Unretained(this))));
}

TEST_F(DeviceSettingsProviderTest, PolicyFailedPermanentlyNotification) {
  Mock::VerifyAndClear(&signed_settings_helper_);
  EXPECT_CALL(signed_settings_helper_, StartRetrievePolicyOp(_))
      .WillRepeatedly(
          MockSignedSettingsHelperRetrievePolicy(
              SignedSettings::OPERATION_FAILED,
              policy_blob_));

  provider_->set_trusted_status(CrosSettingsProvider::TEMPORARILY_UNTRUSTED);
  EXPECT_CALL(*this, GetTrustedCallback());
  EXPECT_EQ(CrosSettingsProvider::TEMPORARILY_UNTRUSTED,
            provider_->PrepareTrustedValues(
                base::Bind(&DeviceSettingsProviderTest::GetTrustedCallback,
                           base::Unretained(this))));
  provider_->Reload();
}

TEST_F(DeviceSettingsProviderTest, PolicyLoadNotification) {
  provider_->set_trusted_status(CrosSettingsProvider::TEMPORARILY_UNTRUSTED);
  EXPECT_CALL(*this, GetTrustedCallback());
  EXPECT_EQ(CrosSettingsProvider::TEMPORARILY_UNTRUSTED,
            provider_->PrepareTrustedValues(
                base::Bind(&DeviceSettingsProviderTest::GetTrustedCallback,
                           base::Unretained(this))));
  provider_->Reload();
}

} // namespace chromeos
