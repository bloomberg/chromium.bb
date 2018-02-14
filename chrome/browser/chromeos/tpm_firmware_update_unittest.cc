// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/tpm_firmware_update.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/important_file_writer.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/scoped_task_environment.h"
#include "base/values.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/chromeos/settings/stub_install_attributes.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace tpm_firmware_update {

TEST(TPMFirmwareUpdateTest, DecodeSettingsProto) {
  enterprise_management::TPMFirmwareUpdateSettingsProto settings;
  settings.set_allow_user_initiated_powerwash(true);
  auto dict = DecodeSettingsProto(settings);
  ASSERT_TRUE(dict);
  bool value = false;
  EXPECT_TRUE(dict->GetBoolean("allow-user-initiated-powerwash", &value));
  EXPECT_TRUE(value);
}

class ShouldOfferUpdateViaPowerwashTest : public testing::Test {
 public:
  enum class Availability {
    kPending,
    kUnavailble,
    kAvailable,
  };

  ShouldOfferUpdateViaPowerwashTest() = default;

  void SetUp() override {
    feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    feature_list_->InitAndEnableFeature(features::kTPMFirmwareUpdate);
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath update_location_path =
        temp_dir_.GetPath().AppendASCII("tpm_firmware_update_location");
    path_override_ = std::make_unique<base::ScopedPathOverride>(
        chrome::FILE_CHROME_OS_TPM_FIRMWARE_UPDATE_LOCATION,
        update_location_path, update_location_path.IsAbsolute(), false);
    SetUpdateAvailability(Availability::kAvailable);
    callback_ =
        base::BindOnce(&ShouldOfferUpdateViaPowerwashTest::RecordResponse,
                       base::Unretained(this));
  }

  void RecordResponse(bool available) {
    callback_received_ = true;
    update_available_ = available;
  }

  void SetUpdateAvailability(Availability availability) {
    base::FilePath update_location_path;
    ASSERT_TRUE(
        PathService::Get(chrome::FILE_CHROME_OS_TPM_FIRMWARE_UPDATE_LOCATION,
                         &update_location_path));
    switch (availability) {
      case Availability::kPending:
        base::DeleteFile(update_location_path, false);
        break;
      case Availability::kUnavailble:
        ASSERT_TRUE(base::ImportantFileWriter::WriteFileAtomically(
            update_location_path, ""));
        break;
      case Availability::kAvailable:
        const char kUpdatePath[] = "/lib/firmware/tpm/firmware.bin";
        ASSERT_TRUE(base::ImportantFileWriter::WriteFileAtomically(
            update_location_path, kUpdatePath));
        break;
    }
  }

  std::unique_ptr<base::test::ScopedFeatureList> feature_list_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::ScopedPathOverride> path_override_;
  base::test::ScopedTaskEnvironment scoped_task_environment_{
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME};
  chromeos::system::ScopedFakeStatisticsProvider statistics_provider_;

  bool callback_received_ = false;
  bool update_available_ = false;
  base::OnceCallback<void(bool)> callback_;
};

TEST_F(ShouldOfferUpdateViaPowerwashTest, FeatureDisabled) {
  feature_list_.reset();
  feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  feature_list_->InitAndDisableFeature(features::kTPMFirmwareUpdate);
  ShouldOfferUpdateViaPowerwash(std::move(callback_), base::TimeDelta());
  EXPECT_TRUE(callback_received_);
  EXPECT_FALSE(update_available_);
}

TEST_F(ShouldOfferUpdateViaPowerwashTest, FRERequired) {
  statistics_provider_.SetMachineStatistic(system::kCheckEnrollmentKey, "1");
  ShouldOfferUpdateViaPowerwash(std::move(callback_), base::TimeDelta());
  EXPECT_TRUE(callback_received_);
  EXPECT_FALSE(update_available_);
}

TEST_F(ShouldOfferUpdateViaPowerwashTest, Pending) {
  SetUpdateAvailability(Availability::kPending);
  ShouldOfferUpdateViaPowerwash(std::move(callback_), base::TimeDelta());
  scoped_task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_received_);
  EXPECT_FALSE(update_available_);
}

TEST_F(ShouldOfferUpdateViaPowerwashTest, Available) {
  ShouldOfferUpdateViaPowerwash(std::move(callback_), base::TimeDelta());
  scoped_task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_received_);
  EXPECT_TRUE(update_available_);
}

TEST_F(ShouldOfferUpdateViaPowerwashTest, AvailableAfterWaiting) {
  SetUpdateAvailability(Availability::kPending);
  ShouldOfferUpdateViaPowerwash(std::move(callback_),
                                base::TimeDelta::FromSeconds(5));
  scoped_task_environment_.RunUntilIdle();
  EXPECT_FALSE(callback_received_);

  // When testing that file appearance triggers the callback, we can't rely on
  // a single execution of ScopedTaskEnvironment::RunUntilIdle(). This is
  // because ScopedTaskEnvironment doesn't know about file system events that
  // haven't fired and propagated to a task scheduler thread yet so may return
  // early before the file system event is received. An event is expected here
  // though, so keep spinning the loop until the callback is received. This
  // isn't ideal, but better than flakiness due to file system events racing
  // with a single invocation of RunUntilIdle().
  SetUpdateAvailability(Availability::kAvailable);
  while (!callback_received_) {
    scoped_task_environment_.RunUntilIdle();
  }
  EXPECT_TRUE(update_available_);

  // Trigger timeout and validate there are no further callbacks or crashes.
  callback_received_ = false;
  scoped_task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(5));
  scoped_task_environment_.RunUntilIdle();
  EXPECT_FALSE(callback_received_);
}

TEST_F(ShouldOfferUpdateViaPowerwashTest, Timeout) {
  SetUpdateAvailability(Availability::kPending);
  ShouldOfferUpdateViaPowerwash(std::move(callback_),
                                base::TimeDelta::FromSeconds(5));
  scoped_task_environment_.RunUntilIdle();
  EXPECT_FALSE(callback_received_);

  scoped_task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(5));
  scoped_task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_received_);
  EXPECT_FALSE(update_available_);
}

class ShouldOfferUpdateViaPowerwashEnterpriseTest
    : public ShouldOfferUpdateViaPowerwashTest {
 public:
  void SetUp() override {
    ShouldOfferUpdateViaPowerwashTest::SetUp();
    cros_settings_test_helper_.ReplaceProvider(kTPMFirmwareUpdateSettings);
  }

  void SetPolicy(bool allowed) {
    base::DictionaryValue dict;
    dict.SetKey(kSettingsKeyAllowPowerwash, base::Value(allowed));
    cros_settings_test_helper_.Set(kTPMFirmwareUpdateSettings, dict);
  }

  ScopedStubInstallAttributes install_attributes_ =
      ScopedStubInstallAttributes::CreateCloudManaged("example.com",
                                                      "fake-device-id");
  ScopedCrosSettingsTestHelper cros_settings_test_helper_;
};

TEST_F(ShouldOfferUpdateViaPowerwashEnterpriseTest, DeviceSettingPending) {
  SetPolicy(true);

  cros_settings_test_helper_.SetTrustedStatus(
      CrosSettingsProvider::TEMPORARILY_UNTRUSTED);
  ShouldOfferUpdateViaPowerwash(std::move(callback_), base::TimeDelta());
  scoped_task_environment_.RunUntilIdle();
  EXPECT_FALSE(callback_received_);

  cros_settings_test_helper_.SetTrustedStatus(
      CrosSettingsProvider::TRUSTED);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_received_);
  EXPECT_TRUE(update_available_);
}

TEST_F(ShouldOfferUpdateViaPowerwashEnterpriseTest, DeviceSettingUntrusted) {
  cros_settings_test_helper_.SetTrustedStatus(
      CrosSettingsProvider::PERMANENTLY_UNTRUSTED);
  ShouldOfferUpdateViaPowerwash(std::move(callback_), base::TimeDelta());
  scoped_task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_received_);
  EXPECT_FALSE(update_available_);
}

TEST_F(ShouldOfferUpdateViaPowerwashEnterpriseTest, DeviceSettingNotSet) {
  ShouldOfferUpdateViaPowerwash(std::move(callback_), base::TimeDelta());
  scoped_task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_received_);
  EXPECT_FALSE(update_available_);
}

TEST_F(ShouldOfferUpdateViaPowerwashEnterpriseTest, DeviceSettingDisallowed) {
  SetPolicy(false);
  ShouldOfferUpdateViaPowerwash(std::move(callback_), base::TimeDelta());
  scoped_task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_received_);
  EXPECT_FALSE(update_available_);
}

TEST_F(ShouldOfferUpdateViaPowerwashEnterpriseTest, DeviceSettingAllowed) {
  SetPolicy(true);
  ShouldOfferUpdateViaPowerwash(std::move(callback_), base::TimeDelta());
  scoped_task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_received_);
  EXPECT_TRUE(update_available_);
}

}  // namespace tpm_firmware_update
}  // namespace chromeos
