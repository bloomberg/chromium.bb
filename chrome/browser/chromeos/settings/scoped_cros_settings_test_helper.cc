// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/ownership/fake_owner_settings_service.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_cache.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/ownership/mock_owner_key_util.h"
#include "policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

ScopedCrosSettingsTestHelper::ScopedCrosSettingsTestHelper() {
  Initialize(true);
}

ScopedCrosSettingsTestHelper::ScopedCrosSettingsTestHelper(
    bool create_settings_service) {
  Initialize(create_settings_service);
}

ScopedCrosSettingsTestHelper::~ScopedCrosSettingsTestHelper() {
  RestoreProvider();
}

scoped_ptr<FakeOwnerSettingsService>
ScopedCrosSettingsTestHelper::CreateOwnerSettingsService(Profile* profile) {
  return make_scoped_ptr(new FakeOwnerSettingsService(
      profile, new ownership::MockOwnerKeyUtil(), &stub_settings_provider_));
}

void ScopedCrosSettingsTestHelper::ReplaceProvider(const std::string& path) {
  CHECK(!real_settings_provider_);
  // Swap out the DeviceSettingsProvider with our settings provider so we can
  // set values for the specified path.
  CrosSettings* const cros_settings = CrosSettings::Get();
  real_settings_provider_ = cros_settings->GetProvider(path);
  EXPECT_TRUE(real_settings_provider_);
  EXPECT_TRUE(cros_settings->RemoveSettingsProvider(real_settings_provider_));
  cros_settings->AddSettingsProvider(&stub_settings_provider_);
}

void ScopedCrosSettingsTestHelper::RestoreProvider() {
  if (real_settings_provider_) {
    // Restore the real DeviceSettingsProvider.
    CrosSettings* const cros_settings = CrosSettings::Get();
    EXPECT_TRUE(
        cros_settings->RemoveSettingsProvider(&stub_settings_provider_));
    cros_settings->AddSettingsProvider(real_settings_provider_);
    real_settings_provider_ = nullptr;
  }
}

void ScopedCrosSettingsTestHelper::SetTrustedStatus(
    CrosSettingsProvider::TrustedStatus status) {
  stub_settings_provider_.SetTrustedStatus(status);
}

void ScopedCrosSettingsTestHelper::SetCurrentUserIsOwner(bool owner) {
  stub_settings_provider_.SetCurrentUserIsOwner(owner);
}

void ScopedCrosSettingsTestHelper::Set(const std::string& path,
                                       const base::Value& in_value) {
  stub_settings_provider_.Set(path, in_value);
}

void ScopedCrosSettingsTestHelper::SetBoolean(const std::string& path,
                                              bool in_value) {
  Set(path, base::FundamentalValue(in_value));
}

void ScopedCrosSettingsTestHelper::SetInteger(const std::string& path,
                                              int in_value) {
  Set(path, base::FundamentalValue(in_value));
}

void ScopedCrosSettingsTestHelper::SetDouble(const std::string& path,
                                             double in_value) {
  Set(path, base::FundamentalValue(in_value));
}

void ScopedCrosSettingsTestHelper::SetString(const std::string& path,
                                             const std::string& in_value) {
  Set(path, base::StringValue(in_value));
}

void ScopedCrosSettingsTestHelper::StoreCachedDeviceSetting(
    const std::string& path) {
  const base::Value* const value = stub_settings_provider_.Get(path);
  if (value) {
    enterprise_management::PolicyData data;
    enterprise_management::ChromeDeviceSettingsProto settings;
    if (device_settings_cache::Retrieve(&data,
                                        g_browser_process->local_state())) {
      CHECK(settings.ParseFromString(data.policy_value()));
    }
    OwnerSettingsServiceChromeOS::UpdateDeviceSettings(path, *value, settings);
    CHECK(settings.SerializeToString(data.mutable_policy_value()));
    CHECK(device_settings_cache::Store(data, g_browser_process->local_state()));
  }
}

void ScopedCrosSettingsTestHelper::CopyStoredValue(const std::string& path) {
  CrosSettingsProvider* provider = real_settings_provider_
                                       ? real_settings_provider_
                                       : CrosSettings::Get()->GetProvider(path);
  const base::Value* const value = provider->Get(path);
  if (value) {
    stub_settings_provider_.Set(path, *value);
  }
}

void ScopedCrosSettingsTestHelper::Initialize(bool create_settings_service) {
  if (create_settings_service) {
    CHECK(!DeviceSettingsService::IsInitialized());
    test_device_settings_service_.reset(new ScopedTestDeviceSettingsService());
    test_cros_settings_.reset(new ScopedTestCrosSettings());
  }
}

}  // namespace chromeos
