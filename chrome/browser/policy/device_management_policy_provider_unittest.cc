// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/device_management_policy_provider.h"
#include "chrome/browser/policy/mock_configuration_policy_store.h"
#include "chrome/browser/policy/mock_device_management_backend.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/policy_constants.h"
#include "chrome/test/device_management_test_util.h"
#include "chrome/test/mock_notification_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

using ::testing::_;
using ::testing::Mock;

class DeviceManagementPolicyProviderTest : public testing::Test {
 public:
  DeviceManagementPolicyProviderTest()
      : ui_thread_(BrowserThread::UI, &loop_),
        file_thread_(BrowserThread::FILE, &loop_) {}

  virtual ~DeviceManagementPolicyProviderTest() {}

  virtual void SetUp() {
    EXPECT_TRUE(storage_dir_.CreateUniqueTempDir());
    CreateNewBackend();
    CreateNewProvider();
  }

  void CreateNewBackend() {
    backend_ = new MockDeviceManagementBackend;
    backend_->AddBooleanPolicy(key::kDisableSpdy, true);
  }

  void CreateNewProvider() {
    provider_.reset(new DeviceManagementPolicyProvider(
        ConfigurationPolicyPrefStore::GetChromePolicyDefinitionList(),
        backend_,
        storage_dir_.path()));
    loop_.RunAllPending();
  }

  void SimulateSuccessfulLoginAndRunPending() {
    loop_.RunAllPending();
    SimulateSuccessfulLogin();
    loop_.RunAllPending();
  }

  void SimulateSuccessfulInitialPolicyFetch() {
    MockConfigurationPolicyStore store;
    backend_->AllShouldSucceed();
    EXPECT_CALL(*backend_, ProcessRegisterRequest(_, _, _, _)).Times(1);
    EXPECT_CALL(*backend_, ProcessPolicyRequest(_, _, _)).Times(1);
    SimulateSuccessfulLoginAndRunPending();
    EXPECT_CALL(store, Apply(kPolicyDisableSpdy, _)).Times(1);
    provider_->Provide(&store);
    ASSERT_EQ(1U, store.policy_map().size());
    Mock::VerifyAndClearExpectations(backend_);
    Mock::VerifyAndClearExpectations(&store);
  }

  virtual void TearDown() {
    loop_.RunAllPending();
  }

 protected:
  MockDeviceManagementBackend* backend_;  // weak
  scoped_ptr<DeviceManagementPolicyProvider> provider_;

 private:
  MessageLoop loop_;
  BrowserThread ui_thread_;
  BrowserThread file_thread_;
  ScopedTempDir storage_dir_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManagementPolicyProviderTest);
};

// If there's no login and no previously-fetched policy, the provider should
// provide an empty policy.
TEST_F(DeviceManagementPolicyProviderTest, InitialProvideNoLogin) {
  MockConfigurationPolicyStore store;
  backend_->AllShouldSucceed();
  EXPECT_CALL(store, Apply(_, _)).Times(0);
  provider_->Provide(&store);
  EXPECT_TRUE(store.policy_map().empty());
}

// If the login is successful and there's no previously-fetched policy, the
// policy should be fetched from the server and should be available the first
// time the Provide method is called.
TEST_F(DeviceManagementPolicyProviderTest, InitialProvideWithLogin) {
  SimulateSuccessfulInitialPolicyFetch();
}

// If the login succeeds but the device management backend is unreachable,
// there should be no policy provided if there's no previously-fetched policy,
TEST_F(DeviceManagementPolicyProviderTest, EmptyProvideWithFailedBackend) {
  MockConfigurationPolicyStore store;
  backend_->AllShouldFail();
  EXPECT_CALL(*backend_, ProcessRegisterRequest(_, _, _, _)).Times(1);
  EXPECT_CALL(*backend_, ProcessPolicyRequest(_, _, _)).Times(0);
  SimulateSuccessfulLoginAndRunPending();
  EXPECT_CALL(store, Apply(kPolicyDisableSpdy, _)).Times(0);
  provider_->Provide(&store);
  EXPECT_TRUE(store.policy_map().empty());
}

// If a policy has been fetched previously, if should be available even before
// the login succeeds or the device management backend is available.
TEST_F(DeviceManagementPolicyProviderTest, SecondProvide) {
  // Pre-fetch and persist a policy
  SimulateSuccessfulInitialPolicyFetch();

  // Simulate a app relaunch by constructing a new provider. Policy should be
  // immediately provided and no refresh should be triggered.
  CreateNewBackend();
  EXPECT_CALL(*backend_, ProcessPolicyRequest(_, _, _)).Times(0);
  CreateNewProvider();
  MockConfigurationPolicyStore store;
  EXPECT_CALL(store, Apply(kPolicyDisableSpdy, _)).Times(1);
  provider_->Provide(&store);
}

// When policy is successfully fetched from the device management server, it
// should force a policy refresh.
TEST_F(DeviceManagementPolicyProviderTest, FetchTriggersRefresh) {
  MockNotificationObserver observer;
  NotificationRegistrar registrar;
  registrar.Add(&observer,
                NotificationType::POLICY_CHANGED,
                NotificationService::AllSources());
  EXPECT_CALL(observer,
              Observe(_, _, _)).Times(1);
  SimulateSuccessfulInitialPolicyFetch();
}

}
