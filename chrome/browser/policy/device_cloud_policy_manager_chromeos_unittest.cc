// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_cloud_policy_manager_chromeos.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/browser/chromeos/settings/mock_owner_key_util.h"
#include "chrome/browser/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/policy/enterprise_install_attributes.h"
#include "chrome/browser/policy/mock_device_management_service.h"
#include "chrome/browser/policy/policy_builder.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_pref_service.h"
#include "content/public/test/test_browser_thread.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
namespace {

class DeviceCloudPolicyManagerChromeOSTest : public testing::Test {
 protected:
  DeviceCloudPolicyManagerChromeOSTest()
      : loop_(MessageLoop::TYPE_UI),
        ui_thread_(content::BrowserThread::UI, &loop_),
        file_thread_(content::BrowserThread::FILE, &loop_),
        owner_key_util_(new chromeos::MockOwnerKeyUtil()),
        cryptohome_library_(chromeos::CryptohomeLibrary::GetImpl(true)),
        install_attributes_(cryptohome_library_.get()),
        store_(new DeviceCloudPolicyStoreChromeOS(&device_settings_service_,
                                                  &install_attributes_)),
        manager_(make_scoped_ptr(store_), &install_attributes_) {}

  virtual void SetUp() OVERRIDE {
    chrome::RegisterLocalState(&local_state_);

    device_settings_service_.Initialize(&device_settings_test_helper_,
                                        owner_key_util_);

    policy_.payload().mutable_metrics_enabled()->set_metrics_enabled(true);
    policy_.Build();

    manager_.Init();
  }

  virtual void TearDown() OVERRIDE {
    manager_.Shutdown();
    device_settings_test_helper_.Flush();
  }

  MessageLoop loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  chromeos::DeviceSettingsTestHelper device_settings_test_helper_;
  DevicePolicyBuilder policy_;

  scoped_refptr<chromeos::MockOwnerKeyUtil> owner_key_util_;
  chromeos::DeviceSettingsService device_settings_service_;
  scoped_ptr<chromeos::CryptohomeLibrary> cryptohome_library_;
  EnterpriseInstallAttributes install_attributes_;

  TestingPrefService local_state_;
  MockDeviceManagementService device_management_service_;

  DeviceCloudPolicyStoreChromeOS* store_;
  DeviceCloudPolicyManagerChromeOS manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceCloudPolicyManagerChromeOSTest);
};

TEST_F(DeviceCloudPolicyManagerChromeOSTest, FreshDevice) {
  device_settings_test_helper_.Flush();
  EXPECT_TRUE(manager_.IsInitializationComplete());

  manager_.Connect(&local_state_, &device_management_service_);

  PolicyBundle bundle;
  EXPECT_TRUE(manager_.policies().Equals(bundle));
}

TEST_F(DeviceCloudPolicyManagerChromeOSTest, EnrolledDevice) {
  ASSERT_EQ(EnterpriseInstallAttributes::LOCK_SUCCESS,
            install_attributes_.LockDevice(PolicyBuilder::kFakeUsername,
                                           DEVICE_MODE_ENTERPRISE,
                                           PolicyBuilder::kFakeDeviceId));
  owner_key_util_->SetPublicKeyFromPrivateKey(policy_.signing_key());
  device_settings_service_.OwnerKeySet(true);
  device_settings_test_helper_.set_policy_blob(policy_.GetBlob());
  device_settings_test_helper_.Flush();
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
  EXPECT_TRUE(manager_.IsInitializationComplete());

  PolicyBundle bundle;
  bundle.Get(POLICY_DOMAIN_CHROME, std::string()).Set(
      key::kDeviceMetricsReportingEnabled, POLICY_LEVEL_MANDATORY,
      POLICY_SCOPE_MACHINE, Value::CreateBooleanValue(true));
  EXPECT_TRUE(manager_.policies().Equals(bundle));

  manager_.Connect(&local_state_, &device_management_service_);
  EXPECT_TRUE(manager_.policies().Equals(bundle));

  manager_.Shutdown();
  EXPECT_TRUE(manager_.policies().Equals(bundle));
}

TEST_F(DeviceCloudPolicyManagerChromeOSTest, ConsumerDevice) {
  owner_key_util_->SetPublicKeyFromPrivateKey(policy_.signing_key());
  device_settings_service_.OwnerKeySet(true);
  device_settings_test_helper_.set_policy_blob(policy_.GetBlob());
  device_settings_test_helper_.Flush();
  EXPECT_EQ(CloudPolicyStore::STATUS_BAD_STATE, store_->status());
  EXPECT_TRUE(manager_.IsInitializationComplete());

  PolicyBundle bundle;
  EXPECT_TRUE(manager_.policies().Equals(bundle));

  manager_.Connect(&local_state_, &device_management_service_);
  EXPECT_TRUE(manager_.policies().Equals(bundle));

  manager_.Shutdown();
  EXPECT_TRUE(manager_.policies().Equals(bundle));
}

}  // namespace
}  // namespace policy
