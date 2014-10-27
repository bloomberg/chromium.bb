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

  virtual ~PrefsChecker() { service_->RemoveObserver(this); }

  // OwnerSettingsService::Observer implementation:
  virtual void OnSignedPolicyStored(bool success) override {
    CHECK(success);

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

  void Set(const std::string& setting, const base::Value& value) {
    service_->Set(setting, value);
    set_requests_.push(
        SetRequest(setting, linked_ptr<base::Value>(value.DeepCopy())));
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
      : local_state_(TestingBrowserProcess::GetGlobal()),
        user_data_dir_override_(chrome::DIR_USER_DATA) {}

  virtual void SetUp() override {
    DeviceSettingsTestBase::SetUp();
    provider_.reset(new DeviceSettingsProvider(base::Bind(&OnPrefChanged),
                                               &device_settings_service_));
    owner_key_util_->SetPrivateKey(device_policy_.GetSigningKey());
    InitOwner(device_policy_.policy_data().username(), true);
    FlushDeviceSettings();
  }

  virtual void TearDown() override { DeviceSettingsTestBase::TearDown(); }

  void TestSingleSet(OwnerSettingsServiceChromeOS* service,
                     const std::string& setting,
                     const base::Value& in_value) {
    PrefsChecker checker(service, provider_.get());
    checker.Set(setting, in_value);
    FlushDeviceSettings();
    checker.Wait();
  }

  ScopedTestingLocalState local_state_;
  scoped_ptr<DeviceSettingsProvider> provider_;
  base::ScopedPathOverride user_data_dir_override_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OwnerSettingsServiceChromeOSTest);
};

TEST_F(OwnerSettingsServiceChromeOSTest, SingleSetTest) {
  OwnerSettingsServiceChromeOS* service =
      OwnerSettingsServiceChromeOSFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(service);
  ASSERT_TRUE(service->IsOwner());
  TestSingleSet(service, kReleaseChannel, base::StringValue("dev-channel"));
  TestSingleSet(service, kReleaseChannel, base::StringValue("beta-channel"));
  TestSingleSet(service, kReleaseChannel, base::StringValue("stable-channel"));
}

TEST_F(OwnerSettingsServiceChromeOSTest, MultipleSetTest) {
  OwnerSettingsServiceChromeOS* service =
      OwnerSettingsServiceChromeOSFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(service);
  ASSERT_TRUE(service->IsOwner());
  base::FundamentalValue allow_guest(false);
  base::StringValue release_channel("stable-channel");
  base::FundamentalValue show_user_names(true);

  PrefsChecker checker(service, provider_.get());

  checker.Set(kAccountsPrefAllowGuest, allow_guest);
  checker.Set(kReleaseChannel, release_channel);
  checker.Set(kAccountsPrefShowUserNamesOnSignIn, show_user_names);

  FlushDeviceSettings();
  checker.Wait();
}

}  // namespace chromeos
