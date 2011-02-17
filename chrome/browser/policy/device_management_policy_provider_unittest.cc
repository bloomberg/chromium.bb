// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/policy/cloud_policy_cache.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/device_management_policy_provider.h"
#include "chrome/browser/policy/mock_configuration_policy_store.h"
#include "chrome/browser/policy/mock_device_management_backend.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/notification_observer_mock.h"
#include "chrome/common/notification_service.h"
#include "chrome/test/testing_device_token_fetcher.h"
#include "chrome/test/testing_profile.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kTestToken[] = "device_policy_provider_test_auth_token";

namespace policy {

using ::testing::_;
using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::Mock;

class MockConfigurationPolicyObserver
    : public ConfigurationPolicyProvider::Observer {
 public:
  MOCK_METHOD0(OnUpdatePolicy, void());
  void OnProviderGoingAway() {}
};

class DeviceManagementPolicyProviderTest : public testing::Test {
 public:
  DeviceManagementPolicyProviderTest()
      : ui_thread_(BrowserThread::UI, &loop_),
        file_thread_(BrowserThread::FILE, &loop_) {}

  virtual ~DeviceManagementPolicyProviderTest() {}

  virtual void SetUp() {
    profile_.reset(new TestingProfile);
    CreateNewProvider();
    EXPECT_TRUE(waiting_for_initial_policies());
    loop_.RunAllPending();
  }

  void CreateNewProvider() {
    backend_ = new MockDeviceManagementBackend;
    provider_.reset(new DeviceManagementPolicyProvider(
        ConfigurationPolicyPrefStore::GetChromePolicyDefinitionList(),
        backend_,
        profile_.get()));
    provider_->SetDeviceTokenFetcher(
        new TestingDeviceTokenFetcher(backend_,
                                      profile_.get(),
                                      provider_->GetTokenPath()));
  }

  void CreateNewProvider(int64 policy_refresh_rate_ms,
                         int policy_refresh_fuzz_factor_percent,
                         int64 policy_refresh_fuzz_max,
                         int64 policy_refresh_error_delay_ms,
                         int64 token_fetch_error_delay_ms,
                         int64 unmanaged_device_refresh_rate_ms) {
    backend_ = new MockDeviceManagementBackend;
    provider_.reset(new DeviceManagementPolicyProvider(
        ConfigurationPolicyPrefStore::GetChromePolicyDefinitionList(),
        backend_,
        profile_.get(),
        policy_refresh_rate_ms,
        policy_refresh_fuzz_factor_percent,
        policy_refresh_fuzz_max,
        policy_refresh_error_delay_ms,
        token_fetch_error_delay_ms,
        unmanaged_device_refresh_rate_ms));
    provider_->SetDeviceTokenFetcher(
        new TestingDeviceTokenFetcher(backend_,
                                      profile_.get(),
                                      provider_->GetTokenPath()));
  }

  FilePath GetTokenPath() const {
    return provider_->GetTokenPath();
  }

  void SimulateSuccessfulLoginAndRunPending() {
    // Make sure the notification for the initial policy fetch is generated.
    MockConfigurationPolicyObserver observer;
    ConfigurationPolicyObserverRegistrar registrar;
    registrar.Init(provider_.get(), &observer);
    EXPECT_CALL(observer, OnUpdatePolicy()).Times(AtLeast(1));

    loop_.RunAllPending();
    profile_->GetTokenService()->IssueAuthTokenForTest(
        GaiaConstants::kDeviceManagementService, kTestToken);
    TestingDeviceTokenFetcher* fetcher =
        static_cast<TestingDeviceTokenFetcher*>(
            provider_->token_fetcher_.get());
    fetcher->SimulateLogin(kTestManagedDomainUsername);
    loop_.RunAllPending();
  }

  void SimulateSuccessfulInitialPolicyFetch() {
    MockConfigurationPolicyStore store;
    EXPECT_CALL(*backend_, ProcessRegisterRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendSucceedRegister());
    EXPECT_CALL(*backend_, ProcessCloudPolicyRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendSucceedSpdyCloudPolicy());
    SimulateSuccessfulLoginAndRunPending();
    EXPECT_FALSE(waiting_for_initial_policies());
    EXPECT_CALL(store, Apply(kPolicyDisableSpdy, _)).Times(1);
    provider_->Provide(&store);
    ASSERT_EQ(1U, store.policy_map().size());
    Mock::VerifyAndClearExpectations(backend_);
    Mock::VerifyAndClearExpectations(&store);
  }

  virtual void TearDown() {
    provider_.reset();
    loop_.RunAllPending();
  }

  bool waiting_for_initial_policies() const {
    return !provider_->IsInitializationComplete();
  }

  MockDeviceManagementBackend* backend_;  // weak
  scoped_ptr<DeviceManagementPolicyProvider> provider_;

 protected:
  CloudPolicyCache* cache(DeviceManagementPolicyProvider* provider) {
    return provider->cache_.get();
  }

  MessageLoop loop_;

 private:
  BrowserThread ui_thread_;
  BrowserThread file_thread_;
  scoped_ptr<Profile> profile_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManagementPolicyProviderTest);
};

// If there's no login and no previously-fetched policy, the provider should
// provide an empty policy.
TEST_F(DeviceManagementPolicyProviderTest, InitialProvideNoLogin) {
  MockConfigurationPolicyStore store;
  EXPECT_CALL(store, Apply(_, _)).Times(0);
  provider_->Provide(&store);
  EXPECT_TRUE(store.policy_map().empty());
  EXPECT_TRUE(waiting_for_initial_policies());
}

// If the login is successful and there's no previously-fetched policy, the
// policy should be fetched from the server and should be available the first
// time the Provide method is called.
TEST_F(DeviceManagementPolicyProviderTest, InitialProvideWithLogin) {
  EXPECT_TRUE(waiting_for_initial_policies());
  SimulateSuccessfulInitialPolicyFetch();
}

// If the login succeed but the device management backend is unreachable,
// there should be no policy provided if there's no previously-fetched policy,
TEST_F(DeviceManagementPolicyProviderTest, EmptyProvideWithFailedBackend) {
  MockConfigurationPolicyStore store;
  EXPECT_CALL(*backend_, ProcessRegisterRequest(_, _, _, _)).WillOnce(
      MockDeviceManagementBackendFailRegister(
          DeviceManagementBackend::kErrorRequestFailed));
  EXPECT_CALL(*backend_, ProcessPolicyRequest(_, _, _, _)).Times(0);
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
  // refreshed (since that might be the purpose of the app relaunch).
  CreateNewProvider();
  EXPECT_CALL(*backend_, ProcessCloudPolicyRequest(_, _, _, _)).WillOnce(
      MockDeviceManagementBackendSucceedSpdyCloudPolicy());
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(backend_);

  // Simulate another app relaunch, this time against a failing backend.
  // Cached policy should still be available.
  MockConfigurationPolicyStore store;
  CreateNewProvider();
  EXPECT_CALL(*backend_, ProcessCloudPolicyRequest(_, _, _, _)).WillOnce(
      MockDeviceManagementBackendFailPolicy(
          DeviceManagementBackend::kErrorRequestFailed));
  SimulateSuccessfulLoginAndRunPending();
  EXPECT_CALL(store, Apply(kPolicyDisableSpdy, _)).Times(1);
  provider_->Provide(&store);
  ASSERT_EQ(1U, store.policy_map().size());
}

// When policy is successfully fetched from the device management server, it
// should force a policy refresh.
TEST_F(DeviceManagementPolicyProviderTest, FetchTriggersRefresh) {
  MockConfigurationPolicyObserver observer;
  ConfigurationPolicyObserverRegistrar registrar;
  registrar.Init(provider_.get(), &observer);
  EXPECT_CALL(observer, OnUpdatePolicy()).Times(1);
  SimulateSuccessfulInitialPolicyFetch();
}

TEST_F(DeviceManagementPolicyProviderTest, ErrorCausesNewRequest) {
  { // Scoping so SimulateSuccessfulLoginAndRunPending doesn't see the sequence.
    InSequence s;
    CreateNewProvider(1000 * 1000, 0, 0, 0, 0, 0);
    EXPECT_CALL(*backend_, ProcessRegisterRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendFailRegister(
            DeviceManagementBackend::kErrorRequestFailed));
    EXPECT_CALL(*backend_, ProcessRegisterRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendSucceedRegister());
    EXPECT_CALL(*backend_, ProcessCloudPolicyRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendFailPolicy(
            DeviceManagementBackend::kErrorRequestFailed));
    EXPECT_CALL(*backend_, ProcessCloudPolicyRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendFailPolicy(
            DeviceManagementBackend::kErrorRequestFailed));
    EXPECT_CALL(*backend_, ProcessCloudPolicyRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendSucceedSpdyCloudPolicy());
  }
  SimulateSuccessfulLoginAndRunPending();
}

TEST_F(DeviceManagementPolicyProviderTest, RefreshPolicies) {
  { // Scoping so SimulateSuccessfulLoginAndRunPending doesn't see the sequence.
    InSequence s;
    CreateNewProvider(0, 0, 0, 1000 * 1000, 1000, 0);
    EXPECT_CALL(*backend_, ProcessRegisterRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendSucceedRegister());
    EXPECT_CALL(*backend_, ProcessCloudPolicyRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendSucceedSpdyCloudPolicy());
    EXPECT_CALL(*backend_, ProcessCloudPolicyRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendSucceedSpdyCloudPolicy());
    EXPECT_CALL(*backend_, ProcessCloudPolicyRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendSucceedSpdyCloudPolicy());
    EXPECT_CALL(*backend_, ProcessCloudPolicyRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendFailPolicy(
            DeviceManagementBackend::kErrorRequestFailed));
  }
  SimulateSuccessfulLoginAndRunPending();
}

// The client should try to re-register the device if the device server reports
// back that it doesn't recognize the device token on a policy request.
TEST_F(DeviceManagementPolicyProviderTest, DeviceNotFound) {
  { // Scoping so SimulateSuccessfulLoginAndRunPending doesn't see the sequence.
    InSequence s;
    EXPECT_CALL(*backend_, ProcessRegisterRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendSucceedRegister());
    EXPECT_CALL(*backend_, ProcessCloudPolicyRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendFailPolicy(
            DeviceManagementBackend::kErrorServiceDeviceNotFound));
    EXPECT_CALL(*backend_, ProcessRegisterRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendSucceedRegister());
    EXPECT_CALL(*backend_, ProcessCloudPolicyRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendSucceedSpdyCloudPolicy());
  }
  SimulateSuccessfulLoginAndRunPending();
}

// The client should try to re-register the device if the device server reports
// back that the device token is invalid on a policy request.
TEST_F(DeviceManagementPolicyProviderTest, InvalidTokenOnPolicyRequest) {
  { // Scoping so SimulateSuccessfulLoginAndRunPending doesn't see the sequence.
    InSequence s;
    EXPECT_CALL(*backend_, ProcessRegisterRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendSucceedRegister());
    EXPECT_CALL(*backend_, ProcessCloudPolicyRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendFailPolicy(
            DeviceManagementBackend::kErrorServiceManagementTokenInvalid));
    EXPECT_CALL(*backend_, ProcessRegisterRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendSucceedRegister());
    EXPECT_CALL(*backend_, ProcessCloudPolicyRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendSucceedSpdyCloudPolicy());
  }
  SimulateSuccessfulLoginAndRunPending();
}

// If the client is successfully managed, but the admin stops managing the
// device, the client should notice and throw away the device token and id.
TEST_F(DeviceManagementPolicyProviderTest, DeviceNoLongerManaged) {
  { // Scoping so SimulateSuccessfulLoginAndRunPending doesn't see the sequence.
    InSequence s;
    CreateNewProvider(0, 0, 0, 0, 0, 1000 * 1000);
    EXPECT_CALL(*backend_, ProcessRegisterRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendSucceedRegister());
    EXPECT_CALL(*backend_, ProcessCloudPolicyRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendSucceedSpdyCloudPolicy());
    EXPECT_CALL(*backend_, ProcessCloudPolicyRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendSucceedSpdyCloudPolicy());
    EXPECT_CALL(*backend_, ProcessCloudPolicyRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendFailPolicy(
            DeviceManagementBackend::kErrorServiceManagementNotSupported));
    EXPECT_CALL(*backend_, ProcessRegisterRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendFailRegister(
            DeviceManagementBackend::kErrorServiceManagementNotSupported));
  }
  SimulateSuccessfulLoginAndRunPending();
  FilePath token_path(GetTokenPath());
  EXPECT_FALSE(file_util::PathExists(token_path));
}

// This test tests three things (see numbered comments below):
TEST_F(DeviceManagementPolicyProviderTest, UnmanagedDevice) {
  { // Scoping so SimulateSuccessfulLoginAndRunPending doesn't see the sequence.
    InSequence s;
    EXPECT_CALL(*backend_, ProcessRegisterRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendFailRegister(
            DeviceManagementBackend::kErrorServiceManagementNotSupported));
  }
  SimulateSuccessfulLoginAndRunPending();
  // (1) The provider's DMPolicyCache should know that the device is not
  // managed.
  EXPECT_TRUE(cache(provider_.get())->is_unmanaged());
  // (2) On restart, the provider should detect that this is not the first
  // login.
  CreateNewProvider(1000 * 1000, 0, 0, 0, 0, 0);
  EXPECT_FALSE(waiting_for_initial_policies());
  { // Scoping so SimulateSuccessfulLoginAndRunPending doesn't see the sequence.
    InSequence s;
    EXPECT_CALL(*backend_, ProcessRegisterRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendSucceedRegister());
    EXPECT_CALL(*backend_, ProcessCloudPolicyRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendSucceedSpdyCloudPolicy());
  }
  SimulateSuccessfulLoginAndRunPending();
  // (3) Since the backend call this time returned a device id, the "unmanaged"
  // marker should have been deleted.
  EXPECT_FALSE(cache(provider_.get())->is_unmanaged());
}

TEST_F(DeviceManagementPolicyProviderTest, FallbackToOldProtocol) {
  { // Scoping so SimulateSuccessfulLoginAndRunPending doesn't see the sequence.
    InSequence s;
    CreateNewProvider(0, 0, 0, 0, 0, 1000 * 1000);
    EXPECT_CALL(*backend_, ProcessRegisterRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendSucceedRegister());
    // If the CloudPolicyRequest fails with kErrorRequestInvalid...
    EXPECT_CALL(*backend_, ProcessCloudPolicyRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendFailPolicy(
            DeviceManagementBackend::kErrorRequestInvalid));
    // ...the client should fall back to a classic PolicyRequest...
    EXPECT_CALL(*backend_, ProcessPolicyRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendSucceedBooleanPolicy(
            key::kDisableSpdy, true));
    // ...and remember this fallback for any future request, ...
    EXPECT_CALL(*backend_, ProcessPolicyRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendFailPolicy(
            DeviceManagementBackend::kErrorHttpStatus));
    // ...both after successful fetches and after errors.
    EXPECT_CALL(*backend_, ProcessPolicyRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendFailPolicy(
            DeviceManagementBackend::kErrorServiceManagementNotSupported));
    // Finally, we set the client to 'unmanaged' to stop its request stream.
    EXPECT_CALL(*backend_, ProcessRegisterRequest(_, _, _, _)).WillOnce(
        MockDeviceManagementBackendFailRegister(
            DeviceManagementBackend::kErrorServiceManagementNotSupported));
  }
  SimulateSuccessfulLoginAndRunPending();
}

}  // namespace policy
