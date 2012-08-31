// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_policy_cache.h"

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/browser/chromeos/settings/mock_owner_key_util.h"
#include "chrome/browser/policy/cloud_policy_data_store.h"
#include "chrome/browser/policy/enterprise_install_attributes.h"
#include "chrome/browser/policy/policy_builder.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "content/public/test/test_browser_thread.h"
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

  MOCK_METHOD1(OnCacheGoingAway, void(CloudPolicyCacheBase*));
  MOCK_METHOD1(OnCacheUpdate, void(CloudPolicyCacheBase*));
};

}  // namespace

class DevicePolicyCacheTest : public testing::Test {
 protected:
  DevicePolicyCacheTest()
      : cryptohome_(chromeos::CryptohomeLibrary::GetImpl(true)),
        owner_key_util_(new chromeos::MockOwnerKeyUtil()),
        install_attributes_(cryptohome_.get()),
        message_loop_(MessageLoop::TYPE_UI),
        ui_thread_(content::BrowserThread::UI, &message_loop_),
        file_thread_(content::BrowserThread::FILE, &message_loop_) {}

  virtual void SetUp() OVERRIDE {
    policy_.payload().mutable_device_policy_refresh_rate()->
        set_device_policy_refresh_rate(120);
    policy_.Build();
    device_settings_test_helper_.set_policy_blob(policy_.GetBlob());

    owner_key_util_->SetPublicKeyFromPrivateKey(policy_.signing_key());

    device_settings_service_.Initialize(&device_settings_test_helper_,
                                        owner_key_util_);

    data_store_.reset(CloudPolicyDataStore::CreateForDevicePolicies());
    cache_.reset(new DevicePolicyCache(data_store_.get(),
                                       &install_attributes_,
                                       &device_settings_service_));
    cache_->AddObserver(&observer_);
  }

  virtual void TearDown() OVERRIDE {
    device_settings_test_helper_.Flush();
    device_settings_service_.Shutdown();

    cache_->RemoveObserver(&observer_);
    cache_.reset();
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
                  policy_.policy_data().username(),
                  DEVICE_MODE_ENTERPRISE,
                  std::string()));
  }

  const Value* GetPolicy(const char* policy_name) {
    return cache_->policy()->GetValue(policy_name);
  }

  MockCloudPolicyCacheObserver observer_;

  scoped_ptr<chromeos::CryptohomeLibrary> cryptohome_;
  scoped_refptr<chromeos::MockOwnerKeyUtil> owner_key_util_;
  chromeos::DeviceSettingsTestHelper device_settings_test_helper_;
  chromeos::DeviceSettingsService device_settings_service_;
  EnterpriseInstallAttributes install_attributes_;

  scoped_ptr<CloudPolicyDataStore> data_store_;
  scoped_ptr<DevicePolicyCache> cache_;
  DevicePolicyBuilder policy_;

  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DevicePolicyCacheTest);
};

TEST_F(DevicePolicyCacheTest, ColdStartup) {
  EXPECT_CALL(observer_, OnCacheUpdate(cache_.get())).Times(0);
  cache_->Load();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OnCacheUpdate(cache_.get()));
  device_settings_service_.Load();
  device_settings_test_helper_.Flush();
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
  policy_.payload().mutable_device_policy_refresh_rate()->
      set_device_policy_refresh_rate(300);
  policy_.Build();
  EXPECT_CALL(observer_, OnCacheUpdate(cache_.get())).Times(0);
  EXPECT_TRUE(cache_->SetPolicy(policy_.policy()));
  cache_->SetFetchingDone();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OnCacheUpdate(cache_.get())).Times(0);
  device_settings_test_helper_.FlushStore();
  Mock::VerifyAndClearExpectations(&observer_);

  // Cache update notification should only fire in the retrieve callback.
  EXPECT_CALL(observer_, OnCacheUpdate(cache_.get()));
  device_settings_test_helper_.Flush();
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
  policy_.policy_data().set_username("another_user@example.com");
  policy_.Build();

  EXPECT_CALL(observer_, OnCacheUpdate(cache_.get()));
  EXPECT_TRUE(cache_->SetPolicy(policy_.policy()));
  device_settings_test_helper_.Flush();
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_EQ(policy_.GetBlob(), device_settings_test_helper_.policy_blob());
}

TEST_F(DevicePolicyCacheTest, SetPolicyOtherUserOtherDomain) {
  MakeEnterpriseDevice();
  Startup();

  // Set new policy information. This should fail because the user is from
  // different domain.
  policy_.policy_data().set_username("foreign_user@hackers.com");
  policy_.Build();
  EXPECT_NE(policy_.GetBlob(), device_settings_test_helper_.policy_blob());

  EXPECT_FALSE(cache_->SetPolicy(policy_.policy()));
  device_settings_test_helper_.Flush();
  EXPECT_NE(policy_.GetBlob(), device_settings_test_helper_.policy_blob());
}

TEST_F(DevicePolicyCacheTest, SetPolicyNonEnterpriseDevice) {
  Startup();

  // Set new policy information. This should fail due to invalid user.
  device_settings_test_helper_.set_policy_blob(std::string());

  EXPECT_FALSE(cache_->SetPolicy(policy_.policy()));
  device_settings_test_helper_.Flush();
  EXPECT_TRUE(device_settings_test_helper_.policy_blob().empty());
}

TEST_F(DevicePolicyCacheTest, SetProxyPolicy) {
  MakeEnterpriseDevice();

  em::DeviceProxySettingsProto proxy_settings;
  proxy_settings.set_proxy_mode("direct");
  proxy_settings.set_proxy_server("http://proxy:8080");
  proxy_settings.set_proxy_pac_url("http://proxy:8080/pac.js");
  proxy_settings.set_proxy_bypass_list("127.0.0.1,example.com");
  policy_.payload().mutable_device_proxy_settings()->CopyFrom(proxy_settings);
  policy_.Build();
  device_settings_test_helper_.set_policy_blob(policy_.GetBlob());
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
  policy_.payload().mutable_open_network_configuration()->
      set_open_network_configuration(fake_config);
  policy_.Build();
  device_settings_test_helper_.set_policy_blob(policy_.GetBlob());
  Startup();

  StringValue expected_config(fake_config);
  EXPECT_TRUE(
      Value::Equals(&expected_config,
                    GetPolicy(key::kDeviceOpenNetworkConfiguration)));
}

}  // namespace policy
