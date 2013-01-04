// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_policy_cache.h"

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/browser/policy/cloud_policy_data_store.h"
#include "chrome/browser/policy/enterprise_install_attributes.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Mock;

namespace em = enterprise_management;

namespace policy {

namespace {

class MockCloudPolicyCacheObserver : public CloudPolicyCacheBase::Observer {
 public:
  virtual ~MockCloudPolicyCacheObserver() {}

  MOCK_METHOD1(OnCacheUpdate, void(CloudPolicyCacheBase*));
};

}  // namespace

class DevicePolicyCacheTest : public chromeos::DeviceSettingsTestBase {
 protected:
  DevicePolicyCacheTest()
      : cryptohome_(chromeos::CryptohomeLibrary::GetImpl(true)),
        install_attributes_(cryptohome_.get()) {}

  virtual void SetUp() OVERRIDE {
    DeviceSettingsTestBase::SetUp();
    device_policy_.payload().mutable_device_policy_refresh_rate()->
        set_device_policy_refresh_rate(120);
    device_policy_.Build();
    device_settings_test_helper_.set_policy_blob(device_policy_.GetBlob());

    data_store_.reset(CloudPolicyDataStore::CreateForDevicePolicies());
    cache_.reset(new DevicePolicyCache(data_store_.get(),
                                       &install_attributes_,
                                       &device_settings_service_));
    cache_->AddObserver(&observer_);
  }

  virtual void TearDown() OVERRIDE {
    cache_->RemoveObserver(&observer_);
    cache_.reset();

    DeviceSettingsTestBase::TearDown();
  }

  void Startup() {
    EXPECT_CALL(observer_, OnCacheUpdate(cache_.get()));
    device_settings_service_.Load();
    device_settings_test_helper_.Flush();
    cache_->Load();
    Mock::VerifyAndClearExpectations(&observer_);
  }

  void MakeEnterpriseDevice() {
    ASSERT_EQ(EnterpriseInstallAttributes::LOCK_SUCCESS,
              install_attributes_.LockDevice(
                  device_policy_.policy_data().username(),
                  DEVICE_MODE_ENTERPRISE,
                  std::string()));
  }

  const Value* GetPolicy(const char* policy_name) {
    return cache_->policy()->GetValue(policy_name);
  }

  MockCloudPolicyCacheObserver observer_;

  scoped_ptr<chromeos::CryptohomeLibrary> cryptohome_;
  EnterpriseInstallAttributes install_attributes_;

  scoped_ptr<CloudPolicyDataStore> data_store_;
  scoped_ptr<DevicePolicyCache> cache_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DevicePolicyCacheTest);
};

TEST_F(DevicePolicyCacheTest, ColdStartup) {
  EXPECT_CALL(observer_, OnCacheUpdate(cache_.get())).Times(0);
  cache_->Load();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OnCacheUpdate(cache_.get()));
  ReloadDeviceSettings();
  Mock::VerifyAndClearExpectations(&observer_);

  base::FundamentalValue expected(120);
  EXPECT_TRUE(Value::Equals(&expected,
                            GetPolicy(key::kDevicePolicyRefreshRate)));
}

TEST_F(DevicePolicyCacheTest, WarmStartup) {
  Startup();

  base::FundamentalValue expected(120);
  EXPECT_TRUE(Value::Equals(&expected,
                            GetPolicy(key::kDevicePolicyRefreshRate)));
}

TEST_F(DevicePolicyCacheTest, SetPolicy) {
  MakeEnterpriseDevice();
  Startup();

  base::FundamentalValue expected(120);
  EXPECT_TRUE(Value::Equals(&expected,
                            GetPolicy(key::kDevicePolicyRefreshRate)));

  // Set new policy information.
  device_policy_.payload().mutable_device_policy_refresh_rate()->
      set_device_policy_refresh_rate(300);
  device_policy_.Build();
  EXPECT_CALL(observer_, OnCacheUpdate(cache_.get())).Times(0);
  EXPECT_TRUE(cache_->SetPolicy(device_policy_.policy()));
  cache_->SetFetchingDone();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OnCacheUpdate(cache_.get())).Times(0);
  device_settings_test_helper_.FlushStore();
  Mock::VerifyAndClearExpectations(&observer_);

  // Cache update notification should only fire in the retrieve callback.
  EXPECT_CALL(observer_, OnCacheUpdate(cache_.get()));
  FlushDeviceSettings();
  Mock::VerifyAndClearExpectations(&observer_);

  base::FundamentalValue updated_expected(300);
  EXPECT_TRUE(Value::Equals(&updated_expected,
                            GetPolicy(key::kDevicePolicyRefreshRate)));

  cache_->RemoveObserver(&observer_);
}

TEST_F(DevicePolicyCacheTest, SetPolicyOtherUserSameDomain) {
  MakeEnterpriseDevice();
  Startup();

  // Set new policy information. This should succeed as the domain is the same.
  device_policy_.policy_data().set_username("another_user@example.com");
  device_policy_.Build();

  EXPECT_CALL(observer_, OnCacheUpdate(cache_.get()));
  EXPECT_TRUE(cache_->SetPolicy(device_policy_.policy()));
  FlushDeviceSettings();
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_EQ(device_policy_.GetBlob(),
            device_settings_test_helper_.policy_blob());
}

TEST_F(DevicePolicyCacheTest, SetPolicyOtherUserOtherDomain) {
  MakeEnterpriseDevice();
  Startup();

  // Set new policy information. This should fail because the user is from
  // different domain.
  device_policy_.policy_data().set_username("foreign_user@hackers.com");
  device_policy_.Build();
  EXPECT_NE(device_policy_.GetBlob(),
            device_settings_test_helper_.policy_blob());

  EXPECT_FALSE(cache_->SetPolicy(device_policy_.policy()));
  FlushDeviceSettings();
  EXPECT_NE(device_policy_.GetBlob(),
            device_settings_test_helper_.policy_blob());
}

TEST_F(DevicePolicyCacheTest, SetPolicyNonEnterpriseDevice) {
  Startup();

  // Set new policy information. This should fail due to invalid user.
  device_settings_test_helper_.set_policy_blob(std::string());

  EXPECT_FALSE(cache_->SetPolicy(device_policy_.policy()));
  FlushDeviceSettings();
  EXPECT_TRUE(device_settings_test_helper_.policy_blob().empty());
}

TEST_F(DevicePolicyCacheTest, SetProxyPolicy) {
  MakeEnterpriseDevice();

  em::DeviceProxySettingsProto proxy_settings;
  proxy_settings.set_proxy_mode("direct");
  proxy_settings.set_proxy_server("http://proxy:8080");
  proxy_settings.set_proxy_pac_url("http://proxy:8080/pac.js");
  proxy_settings.set_proxy_bypass_list("127.0.0.1,example.com");
  device_policy_.payload().mutable_device_proxy_settings()->CopyFrom(
      proxy_settings);
  device_policy_.Build();
  device_settings_test_helper_.set_policy_blob(device_policy_.GetBlob());
  Startup();

  DictionaryValue expected;
  expected.SetString(key::kProxyMode, proxy_settings.proxy_mode());
  expected.SetString(key::kProxyServer, proxy_settings.proxy_server());
  expected.SetString(key::kProxyPacUrl, proxy_settings.proxy_pac_url());
  expected.SetString(key::kProxyBypassList, proxy_settings.proxy_bypass_list());
  EXPECT_TRUE(Value::Equals(&expected, GetPolicy(key::kProxySettings)));
}

TEST_F(DevicePolicyCacheTest, SetDeviceNetworkConfigurationPolicy) {
  MakeEnterpriseDevice();

  std::string fake_config("{ 'NetworkConfigurations': [] }");
  device_policy_.payload().mutable_open_network_configuration()->
      set_open_network_configuration(fake_config);
  device_policy_.Build();
  device_settings_test_helper_.set_policy_blob(device_policy_.GetBlob());
  Startup();

  StringValue expected_config(fake_config);
  EXPECT_TRUE(
      Value::Equals(&expected_config,
                    GetPolicy(key::kDeviceOpenNetworkConfiguration)));
}

}  // namespace policy
