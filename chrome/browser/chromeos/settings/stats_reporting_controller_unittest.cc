// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/stats_reporting_controller.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_cache.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

TestingPrefServiceSimple* RegisterPrefs(TestingPrefServiceSimple* local_state) {
  StatsReportingController::RegisterLocalStatePrefs(local_state->registry());
  device_settings_cache::RegisterPrefs(local_state->registry());
  return local_state;
}

class StatsReportingControllerTest : public testing::Test {
 protected:
  StatsReportingControllerTest() {}
  ~StatsReportingControllerTest() override {}

  void SetUp() override {
    StatsReportingController::Initialize(&local_state_);

    device_policy_.Build();
    fake_session_manager_client_.set_device_policy(device_policy_.GetBlob());

    both_keys->SetPublicKeyFromPrivateKey(*device_policy_.GetSigningKey());
    both_keys->SetPrivateKey(device_policy_.GetSigningKey());
    public_key_only->SetPublicKeyFromPrivateKey(
        *device_policy_.GetSigningKey());
  }

  std::unique_ptr<TestingProfile> CreateUser(
      scoped_refptr<ownership::MockOwnerKeyUtil> keys) {
    OwnerSettingsServiceChromeOSFactory::GetInstance()
        ->SetOwnerKeyUtilForTesting(keys);
    std::unique_ptr<TestingProfile> user = std::make_unique<TestingProfile>();
    OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(user.get())
        ->OnTPMTokenReady(true);
    content::RunAllTasksUntilIdle();
    return user;
  }

  void ExpectThatPendingValueIs(bool expected) {
    bool pending = false;
    EXPECT_TRUE(StatsReportingController::Get()->GetPendingValue(&pending));
    EXPECT_EQ(expected, pending);
  }

  void ExpectThatPendingValueIsNotSet() {
    bool pending = false;
    EXPECT_FALSE(StatsReportingController::Get()->GetPendingValue(&pending));
  }

  void ExpectThatActualValueIs(bool expected) {
    bool actual = false;
    EXPECT_TRUE(StatsReportingController::Get()->GetActualValue(&actual));
    EXPECT_EQ(expected, actual);
  }

  void TearDown() override { StatsReportingController::Shutdown(); }

  content::TestBrowserThreadBundle thread_bundle_{
      content::TestBrowserThreadBundle::IO_MAINLOOP};
  TestingPrefServiceSimple local_state_;
  ScopedStubInstallAttributes scoped_install_attributes_;
  ScopedTestDeviceSettingsService scoped_device_settings_;
  ScopedTestCrosSettings scoped_cros_settings_{RegisterPrefs(&local_state_)};
  FakeSessionManagerClient fake_session_manager_client_;
  policy::DevicePolicyBuilder device_policy_;

  scoped_refptr<ownership::MockOwnerKeyUtil> both_keys{
      base::MakeRefCounted<ownership::MockOwnerKeyUtil>()};
  scoped_refptr<ownership::MockOwnerKeyUtil> public_key_only{
      base::MakeRefCounted<ownership::MockOwnerKeyUtil>()};
  scoped_refptr<ownership::MockOwnerKeyUtil> no_keys{
      base::MakeRefCounted<ownership::MockOwnerKeyUtil>()};
};

TEST_F(StatsReportingControllerTest, GetAndSet_OwnershipUnknown) {
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_UNKNOWN,
            DeviceSettingsService::Get()->GetOwnershipStatus());
  EXPECT_FALSE(StatsReportingController::Get()->IsEnabled());
  ExpectThatPendingValueIsNotSet();
  ExpectThatActualValueIs(false);

  auto user = CreateUser(no_keys);
  StatsReportingController::Get()->SetEnabled(user.get(), true);
  // A pending value is written in case there is no owner, in which case it will
  // be written properly when ownership is taken - but we don't read the
  // pending value, in case there actually is an owner already.
  EXPECT_FALSE(StatsReportingController::Get()->IsEnabled());
  ExpectThatPendingValueIs(true);
  ExpectThatActualValueIs(false);

  StatsReportingController::Get()->SetEnabled(user.get(), false);
  EXPECT_FALSE(StatsReportingController::Get()->IsEnabled());
  ExpectThatPendingValueIs(false);
  ExpectThatActualValueIs(false);
}

TEST_F(StatsReportingControllerTest, GetAndSet_OwnershipNone) {
  DeviceSettingsService::Get()->SetSessionManager(&fake_session_manager_client_,
                                                  no_keys);
  DeviceSettingsService::Get()->Load();
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_NONE,
            DeviceSettingsService::Get()->GetOwnershipStatus());
  EXPECT_FALSE(StatsReportingController::Get()->IsEnabled());
  ExpectThatPendingValueIsNotSet();
  ExpectThatActualValueIs(false);

  // Before the device is owned, the value is written as a pending value:
  auto user = CreateUser(no_keys);
  StatsReportingController::Get()->SetEnabled(user.get(), true);
  EXPECT_TRUE(StatsReportingController::Get()->IsEnabled());
  ExpectThatPendingValueIs(true);
  ExpectThatActualValueIs(false);

  StatsReportingController::Get()->SetEnabled(user.get(), false);
  EXPECT_FALSE(StatsReportingController::Get()->IsEnabled());
  ExpectThatPendingValueIs(false);
  ExpectThatActualValueIs(false);
}

TEST_F(StatsReportingControllerTest, GetAndSet_OwnershipTaken) {
  DeviceSettingsService::Get()->SetSessionManager(&fake_session_manager_client_,
                                                  both_keys);
  auto owner = CreateUser(both_keys);

  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_TAKEN,
            DeviceSettingsService::Get()->GetOwnershipStatus());
  EXPECT_FALSE(StatsReportingController::Get()->IsEnabled());
  ExpectThatPendingValueIsNotSet();
  ExpectThatActualValueIs(false);

  // When the device is owned, the owner can write the actual value:
  StatsReportingController::Get()->SetEnabled(owner.get(), true);
  EXPECT_TRUE(StatsReportingController::Get()->IsEnabled());
  ExpectThatPendingValueIsNotSet();
  ExpectThatActualValueIs(true);

  StatsReportingController::Get()->SetEnabled(owner.get(), false);
  EXPECT_FALSE(StatsReportingController::Get()->IsEnabled());
  ExpectThatPendingValueIsNotSet();
  ExpectThatActualValueIs(false);
}

TEST_F(StatsReportingControllerTest, GetAndSet_OwnershipTaken_NonOwner) {
  DeviceSettingsService::Get()->SetSessionManager(&fake_session_manager_client_,
                                                  both_keys);
  auto owner = CreateUser(both_keys);

  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_TAKEN,
            DeviceSettingsService::Get()->GetOwnershipStatus());
  EXPECT_FALSE(StatsReportingController::Get()->IsEnabled());
  ExpectThatPendingValueIsNotSet();
  ExpectThatActualValueIs(false);

  // Setting value has no effect from a non-owner once device is owned:
  auto non_owner = CreateUser(public_key_only);
  StatsReportingController::Get()->SetEnabled(non_owner.get(), true);
  EXPECT_FALSE(StatsReportingController::Get()->IsEnabled());
  ExpectThatPendingValueIsNotSet();
  ExpectThatActualValueIs(false);
}

TEST_F(StatsReportingControllerTest, SetBeforeOwnershipTaken) {
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_UNKNOWN,
            DeviceSettingsService::Get()->GetOwnershipStatus());
  EXPECT_FALSE(StatsReportingController::Get()->IsEnabled());
  ExpectThatPendingValueIsNotSet();
  ExpectThatActualValueIs(false);

  // Before device is owned, setting the value means writing a pending value:
  auto pre_ownership_user = CreateUser(no_keys);
  StatsReportingController::Get()->SetEnabled(pre_ownership_user.get(), true);
  EXPECT_FALSE(StatsReportingController::Get()->IsEnabled());
  ExpectThatPendingValueIs(true);
  ExpectThatActualValueIs(false);

  DeviceSettingsService::Get()->SetSessionManager(&fake_session_manager_client_,
                                                  both_keys);
  auto owner = CreateUser(both_keys);
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_TAKEN,
            DeviceSettingsService::Get()->GetOwnershipStatus());

  // After device is owned, the value is written to Cros settings.
  StatsReportingController::Get()->OnOwnershipTaken(owner.get());
  EXPECT_TRUE(StatsReportingController::Get()->IsEnabled());
  ExpectThatPendingValueIsNotSet();
  ExpectThatActualValueIs(true);
}

}  // namespace chromeos
