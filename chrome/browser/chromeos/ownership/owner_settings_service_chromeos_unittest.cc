// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>
#include <utility>

#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_path_override.h"
#include "base/values.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/settings/device_settings_provider.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/settings/cros_settings_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace chromeos {

namespace {

void OnPrefChanged(const std::string& /* setting */) {
}

class PrefsChecker : public ownership::OwnerSettingsService::Observer {
 public:
  PrefsChecker(OwnerSettingsServiceChromeOS* service,
               DeviceSettingsProvider* provider)
      : service_(service), provider_(provider) {
    CHECK(service_);
    CHECK(provider_);
    service_->AddObserver(this);
  }

  ~PrefsChecker() override { service_->RemoveObserver(this); }

  // OwnerSettingsService::Observer implementation:
  void OnSignedPolicyStored(bool success) override {
    if (service_->has_pending_changes())
      return;

    while (!set_requests_.empty()) {
      SetRequest request = set_requests_.front();
      set_requests_.pop();
      const base::Value* value = provider_->Get(request.first);
      ASSERT_TRUE(request.second->Equals(value));
    }
    loop_.Quit();
  }

  bool Set(const std::string& setting, const base::Value& value) {
    if (!service_->Set(setting, value))
      return false;
    set_requests_.push(
        SetRequest(setting, linked_ptr<base::Value>(value.DeepCopy())));
    return true;
  }

  void Wait() { loop_.Run(); }

 private:
  OwnerSettingsServiceChromeOS* service_;
  DeviceSettingsProvider* provider_;
  base::RunLoop loop_;

  typedef std::pair<std::string, linked_ptr<base::Value>> SetRequest;
  std::queue<SetRequest> set_requests_;

  DISALLOW_COPY_AND_ASSIGN(PrefsChecker);
};

}  // namespace

class OwnerSettingsServiceChromeOSTest : public DeviceSettingsTestBase {
 public:
  OwnerSettingsServiceChromeOSTest()
      : service_(nullptr),
        local_state_(TestingBrowserProcess::GetGlobal()),
        user_data_dir_override_(chrome::DIR_USER_DATA),
        management_settings_set_(false) {}

  void SetUp() override {
    DeviceSettingsTestBase::SetUp();
    provider_.reset(new DeviceSettingsProvider(base::Bind(&OnPrefChanged),
                                               &device_settings_service_));
    owner_key_util_->SetPrivateKey(device_policy_.GetSigningKey());
    InitOwner(device_policy_.policy_data().username(), true);
    FlushDeviceSettings();

    service_ = OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(
        profile_.get());
    ASSERT_TRUE(service_);
    ASSERT_TRUE(service_->IsOwner());

    device_policy_.policy_data().set_management_mode(
        em::PolicyData::LOCAL_OWNER);
    device_policy_.Build();
    device_settings_test_helper_.set_policy_blob(device_policy_.GetBlob());
    ReloadDeviceSettings();
  }

  void TearDown() override { DeviceSettingsTestBase::TearDown(); }

  void TestSingleSet(OwnerSettingsServiceChromeOS* service,
                     const std::string& setting,
                     const base::Value& in_value) {
    PrefsChecker checker(service, provider_.get());
    checker.Set(setting, in_value);
    FlushDeviceSettings();
    checker.Wait();
  }

  void OnManagementSettingsSet(bool success) {
    management_settings_set_ = success;
  }

  OwnerSettingsServiceChromeOS* service_;
  ScopedTestingLocalState local_state_;
  scoped_ptr<DeviceSettingsProvider> provider_;
  base::ScopedPathOverride user_data_dir_override_;
  bool management_settings_set_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OwnerSettingsServiceChromeOSTest);
};

TEST_F(OwnerSettingsServiceChromeOSTest, SingleSetTest) {
  TestSingleSet(service_, kReleaseChannel, base::StringValue("dev-channel"));
  TestSingleSet(service_, kReleaseChannel, base::StringValue("beta-channel"));
  TestSingleSet(service_, kReleaseChannel, base::StringValue("stable-channel"));
}

TEST_F(OwnerSettingsServiceChromeOSTest, MultipleSetTest) {
  base::FundamentalValue allow_guest(false);
  base::StringValue release_channel("stable-channel");
  base::FundamentalValue show_user_names(true);

  PrefsChecker checker(service_, provider_.get());

  checker.Set(kAccountsPrefAllowGuest, allow_guest);
  checker.Set(kReleaseChannel, release_channel);
  checker.Set(kAccountsPrefShowUserNamesOnSignIn, show_user_names);

  FlushDeviceSettings();
  checker.Wait();
}

TEST_F(OwnerSettingsServiceChromeOSTest, FailedSetRequest) {
  device_settings_test_helper_.set_store_result(false);
  std::string current_channel;
  ASSERT_TRUE(provider_->Get(kReleaseChannel)->GetAsString(&current_channel));
  ASSERT_NE(current_channel, "stable-channel");

  // Check that DeviceSettingsProvider's cache is updated.
  PrefsChecker checker(service_, provider_.get());
  checker.Set(kReleaseChannel, base::StringValue("stable-channel"));
  FlushDeviceSettings();
  checker.Wait();

  // Check that DeviceSettingsService's policy isn't updated.
  ASSERT_EQ(current_channel, device_settings_service_.device_settings()
                                 ->release_channel()
                                 .release_channel());
}

TEST_F(OwnerSettingsServiceChromeOSTest, SetManagementSettingsModeTransition) {
  ReloadDeviceSettings();
  EXPECT_EQ(DeviceSettingsService::STORE_SUCCESS,
            device_settings_service_.status());

  // The initial management mode should be LOCAL_OWNER.
  EXPECT_TRUE(device_settings_service_.policy_data()->has_management_mode());
  EXPECT_EQ(em::PolicyData::LOCAL_OWNER,
            device_settings_service_.policy_data()->management_mode());

  OwnerSettingsServiceChromeOS::ManagementSettings management_settings;
  management_settings.management_mode =
      policy::MANAGEMENT_MODE_CONSUMER_MANAGED;
  management_settings.request_token = "fake_request_token";
  management_settings.device_id = "fake_device_id";
  OwnerSettingsServiceChromeOS::OnManagementSettingsSetCallback
      on_management_settings_set_callback =
          base::Bind(&OwnerSettingsServiceChromeOSTest::OnManagementSettingsSet,
                     base::Unretained(this));

  // LOCAL_OWNER -> CONSUMER_MANAGED: Okay.
  service_->SetManagementSettings(management_settings,
                                  on_management_settings_set_callback);
  FlushDeviceSettings();

  EXPECT_TRUE(management_settings_set_);
  EXPECT_EQ(em::PolicyData::CONSUMER_MANAGED,
            device_settings_service_.policy_data()->management_mode());

  // CONSUMER_MANAGED -> ENTERPRISE_MANAGED: Invalid.
  management_settings.management_mode =
      policy::MANAGEMENT_MODE_ENTERPRISE_MANAGED;
  service_->SetManagementSettings(management_settings,
                                  on_management_settings_set_callback);
  FlushDeviceSettings();

  EXPECT_FALSE(management_settings_set_);
  EXPECT_EQ(em::PolicyData::CONSUMER_MANAGED,
            device_settings_service_.policy_data()->management_mode());

  // CONSUMER_MANAGED -> LOCAL_OWNER: Okay.
  management_settings.management_mode = policy::MANAGEMENT_MODE_LOCAL_OWNER;
  service_->SetManagementSettings(management_settings,
                                  on_management_settings_set_callback);
  FlushDeviceSettings();

  EXPECT_TRUE(management_settings_set_);
  EXPECT_EQ(em::PolicyData::LOCAL_OWNER,
            device_settings_service_.policy_data()->management_mode());

  // LOCAL_OWNER -> ENTERPRISE_MANAGED: Invalid.
  management_settings.management_mode =
      policy::MANAGEMENT_MODE_ENTERPRISE_MANAGED;
  service_->SetManagementSettings(management_settings,
                                  on_management_settings_set_callback);
  FlushDeviceSettings();

  EXPECT_FALSE(management_settings_set_);
  EXPECT_EQ(em::PolicyData::LOCAL_OWNER,
            device_settings_service_.policy_data()->management_mode());

  // Inject a policy data with management mode set to ENTERPRISE_MANAGED.
  device_policy_.policy_data().set_management_mode(
      em::PolicyData::ENTERPRISE_MANAGED);
  device_policy_.Build();
  device_settings_test_helper_.set_policy_blob(device_policy_.GetBlob());
  ReloadDeviceSettings();
  EXPECT_EQ(em::PolicyData::ENTERPRISE_MANAGED,
            device_settings_service_.policy_data()->management_mode());

  // ENTERPRISE_MANAGED -> LOCAL_OWNER: Invalid.
  management_settings.management_mode = policy::MANAGEMENT_MODE_LOCAL_OWNER;
  service_->SetManagementSettings(management_settings,
                                  on_management_settings_set_callback);
  FlushDeviceSettings();

  EXPECT_FALSE(management_settings_set_);
  EXPECT_EQ(em::PolicyData::ENTERPRISE_MANAGED,
            device_settings_service_.policy_data()->management_mode());

  // ENTERPRISE_MANAGED -> CONSUMER_MANAGED: Invalid.
  management_settings.management_mode =
      policy::MANAGEMENT_MODE_CONSUMER_MANAGED;
  service_->SetManagementSettings(management_settings,
                                  on_management_settings_set_callback);
  FlushDeviceSettings();

  EXPECT_FALSE(management_settings_set_);
  EXPECT_EQ(em::PolicyData::ENTERPRISE_MANAGED,
            device_settings_service_.policy_data()->management_mode());
}

TEST_F(OwnerSettingsServiceChromeOSTest, SetManagementSettingsSuccess) {
  ReloadDeviceSettings();
  EXPECT_EQ(DeviceSettingsService::STORE_SUCCESS,
            device_settings_service_.status());

  OwnerSettingsServiceChromeOS::ManagementSettings management_settings;
  management_settings.management_mode =
      policy::MANAGEMENT_MODE_CONSUMER_MANAGED;
  management_settings.request_token = "fake_request_token";
  management_settings.device_id = "fake_device_id";
  service_->SetManagementSettings(
      management_settings,
      base::Bind(&OwnerSettingsServiceChromeOSTest::OnManagementSettingsSet,
                 base::Unretained(this)));
  FlushDeviceSettings();

  EXPECT_EQ(DeviceSettingsService::STORE_SUCCESS,
            device_settings_service_.status());
  ASSERT_TRUE(device_settings_service_.device_settings());

  // Check that the loaded policy_data contains the expected values.
  const em::PolicyData* policy_data = device_settings_service_.policy_data();
  EXPECT_EQ(policy::dm_protocol::kChromeDevicePolicyType,
            policy_data->policy_type());
  EXPECT_EQ(device_settings_service_.GetUsername(), policy_data->username());
  EXPECT_EQ(em::PolicyData::CONSUMER_MANAGED, policy_data->management_mode());
  EXPECT_EQ("fake_request_token", policy_data->request_token());
  EXPECT_EQ("fake_device_id", policy_data->device_id());
}

class OwnerSettingsServiceChromeOSNoOwnerTest
    : public OwnerSettingsServiceChromeOSTest {
 public:
  OwnerSettingsServiceChromeOSNoOwnerTest() {}
  ~OwnerSettingsServiceChromeOSNoOwnerTest() override {}

  void SetUp() override {
    DeviceSettingsTestBase::SetUp();
    provider_.reset(new DeviceSettingsProvider(base::Bind(&OnPrefChanged),
                                               &device_settings_service_));
    FlushDeviceSettings();
    service_ = OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(
        profile_.get());
    ASSERT_TRUE(service_);
    ASSERT_FALSE(service_->IsOwner());
  }

  void TearDown() override { DeviceSettingsTestBase::TearDown(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(OwnerSettingsServiceChromeOSNoOwnerTest);
};

TEST_F(OwnerSettingsServiceChromeOSNoOwnerTest, SingleSetTest) {
  ASSERT_FALSE(service_->SetBoolean(kAccountsPrefAllowGuest, false));
}

}  // namespace chromeos
