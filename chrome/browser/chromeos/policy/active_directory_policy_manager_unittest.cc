// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/active_directory_policy_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromeos/dbus/auth_policy_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/signin/core/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestAuthPolicyClient : public chromeos::AuthPolicyClient {
 public:
  void Init(dbus::Bus* bus) override { NOTIMPLEMENTED(); }

  void JoinAdDomain(const authpolicy::JoinDomainRequest& request,
                    int password_fd,
                    JoinCallback callback) override {
    NOTIMPLEMENTED();
  }

  void AuthenticateUser(const authpolicy::AuthenticateUserRequest& request,
                        int password_fd,
                        AuthCallback callback) override {
    NOTIMPLEMENTED();
  }

  void GetUserStatus(const std::string& object_guid,
                     GetUserStatusCallback callback) override {
    NOTIMPLEMENTED();
  }

  void GetUserKerberosFiles(const std::string& object_guid,
                            GetUserKerberosFilesCallback callback) override {
    NOTIMPLEMENTED();
  }

  void RefreshDevicePolicy(RefreshPolicyCallback callback) override {
    NOTIMPLEMENTED();
  }

  void RefreshUserPolicy(const AccountId& account_id,
                         RefreshPolicyCallback callback) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  refresh_user_policy_callback_error_));
  }

  void ConnectToSignal(
      const std::string& signal_name,
      dbus::ObjectProxy::SignalCallback signal_callback,
      dbus::ObjectProxy::OnConnectedCallback on_connected_callback) override {
    NOTIMPLEMENTED();
  }

  void SetRefreshUserPolicyCallbackError(authpolicy::ErrorType error) {
    refresh_user_policy_callback_error_ = error;
  }

 private:
  authpolicy::ErrorType refresh_user_policy_callback_error_ =
      authpolicy::ERROR_NONE;
};

}  // namespace

namespace policy {

// Note that session exit is asynchronous and thus ActiveDirectoryPolicyManager
// still needs to react reasonably to events happening after session exit has
// been fired.
class ActiveDirectoryPolicyManagerTest : public testing::Test {
 public:
  ActiveDirectoryPolicyManagerTest() {
    auto mock_client_unique_ptr = base::MakeUnique<TestAuthPolicyClient>();
    mock_client_ = mock_client_unique_ptr.get();
    chromeos::DBusThreadManager::GetSetterForTesting()->SetAuthPolicyClient(
        std::move(mock_client_unique_ptr));
  }

  ~ActiveDirectoryPolicyManagerTest() override {
    EXPECT_EQ(session_exit_expected_, session_exited_);
    policy_manager_->Shutdown();
  }

 protected:
  // Expect that session exit will be called below. (Must only be called once.)
  void ExpectSessionExit() {
    ASSERT_FALSE(session_exit_expected_);
    EXPECT_FALSE(session_exited_);
    session_exit_expected_ = true;
  }

  // Expect that session exit has been called above. (Must only be called after
  // ExpectSessionExit().)
  void ExpectSessionExited() {
    ASSERT_TRUE(session_exit_expected_);
    EXPECT_TRUE(session_exited_);
  }

  // Closure to exit the session.
  void ExitSession() {
    EXPECT_TRUE(session_exit_expected_);
    session_exited_ = true;
  }

  bool session_exited_ = false;
  bool session_exit_expected_ = false;

  // To be handed over to ActiveDirectoryPolicyManager.
  base::OnceClosure exit_session_{
      base::BindOnce(&ActiveDirectoryPolicyManagerTest::ExitSession,
                     base::Unretained(this))};

  // To be handed over to ActiveDirectoryPolicyManager ...
  std::unique_ptr<MockCloudPolicyStore> mock_store_unique_ptr_{
      base::MakeUnique<MockCloudPolicyStore>()};

  // ... but keeping a non-owned pointer.
  MockCloudPolicyStore* mock_store_{mock_store_unique_ptr_.get()};

  // Owned by DBusThreadManager.
  TestAuthPolicyClient* mock_client_;

  SchemaRegistry schema_registry_;

  // Initialized by the individual tests but owned by the test class so that it
  // can be shut down automatically after the test has run.
  std::unique_ptr<ActiveDirectoryPolicyManager> policy_manager_;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

TEST_F(ActiveDirectoryPolicyManagerTest, DontWait) {
  policy_manager_ = ActiveDirectoryPolicyManager::CreateForUserPolicy(
      AccountId::AdFromUserEmailObjGuid("bla", "ble"), base::TimeDelta(),
      std::move(exit_session_), std::move(mock_store_unique_ptr_));
  EXPECT_TRUE(policy_manager_);
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Configure mock policy fetch to fail.
  mock_client_->SetRefreshUserPolicyCallbackError(authpolicy::ERROR_UNKNOWN);

  // Trigger mock policy fetch from authpolicyd.
  policy_manager_->Init(&schema_registry_);
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Simulate failed store load. Initialization is reported complete at this
  // point.
  mock_store_->NotifyStoreError();
  EXPECT_TRUE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Process reply for mock policy fetch.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Simulate failed store load.
  mock_store_->NotifyStoreError();
  EXPECT_TRUE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
}

// If the initial fetch timeout is infinite, initialization is only complete
// after policy has been fetched and after that has been loaded.
TEST_F(ActiveDirectoryPolicyManagerTest,
       WaitInfinite_LoadSuccess_FetchSuccess) {
  policy_manager_ = ActiveDirectoryPolicyManager::CreateForUserPolicy(
      AccountId::AdFromUserEmailObjGuid("bla", "ble"), base::TimeDelta::Max(),
      std::move(exit_session_), std::move(mock_store_unique_ptr_));
  ASSERT_TRUE(policy_manager_);
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Configure mock policy fetch to succeed.
  mock_client_->SetRefreshUserPolicyCallbackError(authpolicy::ERROR_NONE);

  // Trigger mock policy fetch from authpolicyd.
  policy_manager_->Init(&schema_registry_);

  // Simulate successful store load.
  mock_store_->policy_ = base::MakeUnique<enterprise_management::PolicyData>();
  mock_store_->NotifyStoreLoaded();
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Process reply for mock policy fetch.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Simulate successful store load. At this point initialization is complete.
  mock_store_->NotifyStoreLoaded();
  EXPECT_TRUE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
}

// If the initial fetch timeout is infinite, initialization does not complete if
// load after fetch fails.
TEST_F(ActiveDirectoryPolicyManagerTest,
       WaitInfinite_LoadSuccess_FetchSuccess_LoadFail) {
  policy_manager_ = ActiveDirectoryPolicyManager::CreateForUserPolicy(
      AccountId::AdFromUserEmailObjGuid("bla", "ble"), base::TimeDelta::Max(),
      std::move(exit_session_), std::move(mock_store_unique_ptr_));
  ASSERT_TRUE(policy_manager_);
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Configure mock policy fetch to succeed.
  mock_client_->SetRefreshUserPolicyCallbackError(authpolicy::ERROR_NONE);

  // Trigger mock policy fetch from authpolicyd.
  policy_manager_->Init(&schema_registry_);

  // Simulate successful store load.
  mock_store_->policy_ = base::MakeUnique<enterprise_management::PolicyData>();
  mock_store_->NotifyStoreLoaded();
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Process reply for mock policy fetch.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  ExpectSessionExit();

  // Simulate failed store load.
  mock_store_->NotifyStoreError();
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  ExpectSessionExited();

  // Simulate successful store load.
  mock_store_->NotifyStoreLoaded();
  EXPECT_TRUE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
}

// If the initial fetch timeout is infinite, failure in policy fetch prevents
// initialization from finishing, ever.
TEST_F(ActiveDirectoryPolicyManagerTest, WaitInfinite_LoadSuccess_FetchFail) {
  policy_manager_ = ActiveDirectoryPolicyManager::CreateForUserPolicy(
      AccountId::AdFromUserEmailObjGuid("bla", "ble"), base::TimeDelta::Max(),
      std::move(exit_session_), std::move(mock_store_unique_ptr_));
  ASSERT_TRUE(policy_manager_);
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Configure mock policy fetch to fail.
  mock_client_->SetRefreshUserPolicyCallbackError(authpolicy::ERROR_UNKNOWN);

  // Trigger mock policy fetch from authpolicyd.
  policy_manager_->Init(&schema_registry_);

  // Simulate successful store load.
  mock_store_->policy_ = base::MakeUnique<enterprise_management::PolicyData>();
  mock_store_->NotifyStoreLoaded();
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  ExpectSessionExit();

  // Process reply for mock policy fetch.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  ExpectSessionExited();

  // Simulate successful store load.
  mock_store_->NotifyStoreLoaded();
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
}

// If the initial fetch timeout is not infinite, we're in best-effort mode but
// still require the policy load to succeed so that there's *some* policy
// present (though possibly outdated).
TEST_F(ActiveDirectoryPolicyManagerTest, WaitFinite_LoadSuccess_FetchFail) {
  policy_manager_ = ActiveDirectoryPolicyManager::CreateForUserPolicy(
      AccountId::AdFromUserEmailObjGuid("bla", "ble"),
      base::TimeDelta::FromDays(365), std::move(exit_session_),
      std::move(mock_store_unique_ptr_));
  ASSERT_TRUE(policy_manager_);
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Configure mock policy fetch to fail.
  mock_client_->SetRefreshUserPolicyCallbackError(authpolicy::ERROR_UNKNOWN);

  // Trigger mock policy fetch from authpolicyd.
  policy_manager_->Init(&schema_registry_);

  // Simulate successful store load.
  mock_store_->policy_ = base::MakeUnique<enterprise_management::PolicyData>();
  mock_store_->NotifyStoreLoaded();
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Process reply for mock policy fetch. At this point initialization is
  // complete (we have waited for the fetch but now that we know it has failed
  // we continue).
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Simulate successful store load.
  mock_store_->NotifyStoreLoaded();
  EXPECT_TRUE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
}

// If the initial fetch timeout is not infinite, we're in best-effort mode but
// still require the policy load to succeed so that there's *some* policy
// present (though possibly outdated). Here the sequence is inverted: Fetch
// returns before load.
TEST_F(ActiveDirectoryPolicyManagerTest, WaitFinite_FetchFail_LoadSuccess) {
  policy_manager_ = ActiveDirectoryPolicyManager::CreateForUserPolicy(
      AccountId::AdFromUserEmailObjGuid("bla", "ble"),
      base::TimeDelta::FromDays(365), std::move(exit_session_),
      std::move(mock_store_unique_ptr_));
  ASSERT_TRUE(policy_manager_);
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Configure mock policy fetch to fail.
  mock_client_->SetRefreshUserPolicyCallbackError(authpolicy::ERROR_UNKNOWN);

  // Trigger mock policy fetch from authpolicyd.
  policy_manager_->Init(&schema_registry_);

  // Process reply for mock policy fetch.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Simulate successful store load.
  mock_store_->policy_ = base::MakeUnique<enterprise_management::PolicyData>();
  mock_store_->NotifyStoreLoaded();
  EXPECT_TRUE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
}

// If the initial fetch timeout is not infinite, we're in best-effort mode but
// if we can't load existing policy from disk we have to give up.
TEST_F(ActiveDirectoryPolicyManagerTest, WaitFinite_LoadFail_FetchFail) {
  policy_manager_ = ActiveDirectoryPolicyManager::CreateForUserPolicy(
      AccountId::AdFromUserEmailObjGuid("bla", "ble"),
      base::TimeDelta::FromDays(365), std::move(exit_session_),
      std::move(mock_store_unique_ptr_));
  ASSERT_TRUE(policy_manager_);
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Configure mock policy fetch to fail.
  mock_client_->SetRefreshUserPolicyCallbackError(authpolicy::ERROR_UNKNOWN);

  // Trigger mock policy fetch from authpolicyd.
  policy_manager_->Init(&schema_registry_);

  // Simulate failed store load.
  mock_store_->NotifyStoreError();
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  ExpectSessionExit();

  // Process reply for mock policy fetch.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  ExpectSessionExited();

  // Simulate successful store load.
  mock_store_->policy_ = base::MakeUnique<enterprise_management::PolicyData>();
  mock_store_->NotifyStoreLoaded();
  EXPECT_TRUE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
}

// If the initial fetch timeout is not infinite, we're in best-effort mode and
// upon timeout initialization is complete if any policy could be loaded from
// disk.
TEST_F(ActiveDirectoryPolicyManagerTest, WaitFinite_LoadSuccess_FetchTimeout) {
  policy_manager_ = ActiveDirectoryPolicyManager::CreateForUserPolicy(
      AccountId::AdFromUserEmailObjGuid("bla", "ble"),
      base::TimeDelta::FromDays(365), std::move(exit_session_),
      std::move(mock_store_unique_ptr_));
  ASSERT_TRUE(policy_manager_);
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Configure mock policy fetch to fail.
  mock_client_->SetRefreshUserPolicyCallbackError(authpolicy::ERROR_UNKNOWN);

  // Trigger mock policy fetch from authpolicyd.
  policy_manager_->Init(&schema_registry_);

  // Simulate successful store load.
  mock_store_->policy_ = base::MakeUnique<enterprise_management::PolicyData>();
  mock_store_->NotifyStoreLoaded();
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Simulate policy fetch timeout.
  policy_manager_->ForceTimeoutForTest();
  EXPECT_TRUE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Process reply for mock policy fetch.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Simulate failed store load.
  mock_store_->NotifyStoreError();
  EXPECT_TRUE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
}

// If the initial fetch timeout is not infinite, we're in best-effort mode but
// without a successful policy load we still can't continue.
TEST_F(ActiveDirectoryPolicyManagerTest, WaitFinite_LoadTimeout_FetchTimeout) {
  policy_manager_ = ActiveDirectoryPolicyManager::CreateForUserPolicy(
      AccountId::AdFromUserEmailObjGuid("bla", "ble"),
      base::TimeDelta::FromDays(365), std::move(exit_session_),
      std::move(mock_store_unique_ptr_));
  ASSERT_TRUE(policy_manager_);
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Configure mock policy fetch to fail.
  mock_client_->SetRefreshUserPolicyCallbackError(authpolicy::ERROR_UNKNOWN);

  // Trigger mock policy fetch from authpolicyd.
  policy_manager_->Init(&schema_registry_);

  ExpectSessionExit();

  // Simulate policy fetch timeout.
  policy_manager_->ForceTimeoutForTest();
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  ExpectSessionExited();

  // Process reply for mock policy fetch.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Simulate successful store load.
  mock_store_->policy_ = base::MakeUnique<enterprise_management::PolicyData>();
  mock_store_->NotifyStoreLoaded();
  EXPECT_TRUE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
}

}  // namespace policy
