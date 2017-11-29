// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/active_directory_policy_manager.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chromeos/dbus/auth_policy_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"

namespace {

// Fetch policy every 90 minutes which matches the Windows default:
// https://technet.microsoft.com/en-us/library/cc940895.aspx
constexpr base::TimeDelta kFetchInterval = base::TimeDelta::FromMinutes(90);

void RunRefreshCallback(base::OnceCallback<void(bool success)> callback,
                        authpolicy::ErrorType error) {
  std::move(callback).Run(error == authpolicy::ERROR_NONE);
}

}  // namespace

namespace policy {

ActiveDirectoryPolicyManager::~ActiveDirectoryPolicyManager() {}

// static
std::unique_ptr<ActiveDirectoryPolicyManager>
ActiveDirectoryPolicyManager::CreateForDevicePolicy(
    std::unique_ptr<CloudPolicyStore> store) {
  // Can't use MakeUnique<> because the constructor is private.
  return base::WrapUnique(
      new ActiveDirectoryPolicyManager(EmptyAccountId(), base::TimeDelta(),
                                       base::OnceClosure(), std::move(store)));
}

// static
std::unique_ptr<ActiveDirectoryPolicyManager>
ActiveDirectoryPolicyManager::CreateForUserPolicy(
    const AccountId& account_id,
    base::TimeDelta initial_policy_fetch_timeout,
    base::OnceClosure exit_session,
    std::unique_ptr<CloudPolicyStore> store) {
  // Can't use MakeUnique<> because the constructor is private.
  return base::WrapUnique(new ActiveDirectoryPolicyManager(
      account_id, initial_policy_fetch_timeout, std::move(exit_session),
      std::move(store)));
}

void ActiveDirectoryPolicyManager::Init(SchemaRegistry* registry) {
  ConfigurationPolicyProvider::Init(registry);

  store_->AddObserver(this);
  if (!store_->is_initialized()) {
    store_->Load();
  }

  // Does nothing if |store_| hasn't yet initialized.
  PublishPolicy();

  scheduler_ = base::MakeUnique<PolicyScheduler>(
      base::BindRepeating(&ActiveDirectoryPolicyManager::DoPolicyFetch,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&ActiveDirectoryPolicyManager::OnPolicyFetched,
                          weak_ptr_factory_.GetWeakPtr()),
      kFetchInterval);
}

void ActiveDirectoryPolicyManager::Shutdown() {
  store_->RemoveObserver(this);
  ConfigurationPolicyProvider::Shutdown();
}

bool ActiveDirectoryPolicyManager::IsInitializationComplete(
    PolicyDomain domain) const {
  if (waiting_for_initial_policy_fetch_) {
    return false;
  }
  if (domain == POLICY_DOMAIN_CHROME) {
    return store_->is_initialized();
  }
  return true;
}

void ActiveDirectoryPolicyManager::RefreshPolicies() {
  scheduler_->ScheduleTaskNow();
}

void ActiveDirectoryPolicyManager::OnStoreLoaded(
    CloudPolicyStore* cloud_policy_store) {
  DCHECK_EQ(store_.get(), cloud_policy_store);
  PublishPolicy();
  if (fetch_ever_completed_) {
    // Policy is guaranteed to be up to date with the previous fetch result
    // because OnPolicyFetched() cancels any potentially running Load()
    // operations.
    CancelWaitForInitialPolicy(fetch_ever_succeeded_ /* success */);
  }
}

void ActiveDirectoryPolicyManager::OnStoreError(
    CloudPolicyStore* cloud_policy_store) {
  DCHECK_EQ(store_.get(), cloud_policy_store);
  // Publish policy (even though it hasn't changed) in order to signal load
  // complete on the ConfigurationPolicyProvider interface. Technically, this is
  // only required on the first load, but doesn't hurt in any case.
  PublishPolicy();
  if (fetch_ever_completed_) {
    CancelWaitForInitialPolicy(false /* success */);
  }
}

void ActiveDirectoryPolicyManager::ForceTimeoutForTest() {
  DCHECK(initial_policy_timeout_.IsRunning());
  // Stop the timer to mimic what happens when a real timer fires, then invoke
  // the timer callback directly.
  initial_policy_timeout_.Stop();
  OnBlockingFetchTimeout();
}

ActiveDirectoryPolicyManager::ActiveDirectoryPolicyManager(
    const AccountId& account_id,
    base::TimeDelta initial_policy_fetch_timeout,
    base::OnceClosure exit_session,
    std::unique_ptr<CloudPolicyStore> store)
    : account_id_(account_id),
      waiting_for_initial_policy_fetch_(
          !initial_policy_fetch_timeout.is_zero()),
      initial_policy_fetch_may_fail_(!initial_policy_fetch_timeout.is_max()),
      exit_session_(std::move(exit_session)),
      store_(std::move(store)) {
  // Delaying initialization complete is intended for user policy only.
  DCHECK(account_id != EmptyAccountId() || !waiting_for_initial_policy_fetch_);
  if (waiting_for_initial_policy_fetch_ && initial_policy_fetch_may_fail_) {
    initial_policy_timeout_.Start(
        FROM_HERE, initial_policy_fetch_timeout,
        base::Bind(&ActiveDirectoryPolicyManager::OnBlockingFetchTimeout,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void ActiveDirectoryPolicyManager::PublishPolicy() {
  if (!store_->is_initialized()) {
    return;
  }
  std::unique_ptr<PolicyBundle> bundle = base::MakeUnique<PolicyBundle>();
  PolicyMap& policy_map =
      bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  policy_map.CopyFrom(store_->policy_map());

  // Overwrite the source which is POLICY_SOURCE_CLOUD by default.
  // TODO(tnagel): Rename CloudPolicyStore to PolicyStore and make the source
  // configurable, then drop PolicyMap::SetSourceForAll().
  policy_map.SetSourceForAll(POLICY_SOURCE_ACTIVE_DIRECTORY);
  SetEnterpriseUsersDefaults(&policy_map);
  UpdatePolicy(std::move(bundle));
}

void ActiveDirectoryPolicyManager::DoPolicyFetch(
    base::OnceCallback<void(bool success)> callback) {
  chromeos::DBusThreadManager* thread_manager =
      chromeos::DBusThreadManager::Get();
  DCHECK(thread_manager);
  chromeos::AuthPolicyClient* auth_policy_client =
      thread_manager->GetAuthPolicyClient();
  DCHECK(auth_policy_client);
  if (account_id_ == EmptyAccountId()) {
    auth_policy_client->RefreshDevicePolicy(
        base::BindOnce(&RunRefreshCallback, std::move(callback)));
  } else {
    auth_policy_client->RefreshUserPolicy(
        account_id_, base::BindOnce(&RunRefreshCallback, std::move(callback)));
  }
}

void ActiveDirectoryPolicyManager::OnPolicyFetched(bool success) {
  fetch_ever_completed_ = true;
  if (success) {
    fetch_ever_succeeded_ = true;
  } else {
    LOG(ERROR) << "Active Directory policy fetch failed.";
    if (store_->is_initialized()) {
      CancelWaitForInitialPolicy(false /* success */);
    }
  }
  // Load independently of success or failure to keep in sync with the state in
  // session manager. This cancels any potentially running Load() operations
  // thus it is guaranteed that at the next OnStoreLoaded() invocation the
  // policy is up-to-date with what was fetched.
  store_->Load();
}

void ActiveDirectoryPolicyManager::OnBlockingFetchTimeout() {
  DCHECK(waiting_for_initial_policy_fetch_);
  LOG(WARNING) << "Timed out while waiting for the policy fetch. "
               << "The session will start with the cached policy.";
  CancelWaitForInitialPolicy(false);
}

void ActiveDirectoryPolicyManager::CancelWaitForInitialPolicy(bool success) {
  if (!waiting_for_initial_policy_fetch_)
    return;

  initial_policy_timeout_.Stop();

  // If the conditions to continue profile initialization are not met, the user
  // session is exited and initialization is not set as completed.
  // TODO(tnagel): Maybe add code to retry policy fetch?
  if (!store_->has_policy()) {
    // If there's no policy at all (not even cached) the user session must not
    // continue.
    LOG(ERROR) << "Policy could not be obtained. "
               << "Aborting profile initialization";
    // Prevent duplicate exit session calls.
    if (exit_session_) {
      std::move(exit_session_).Run();
    }
    return;
  }
  if (!success && !initial_policy_fetch_may_fail_) {
    LOG(ERROR) << "Policy fetch failed for the user. "
               << "Aborting profile initialization";
    // Prevent duplicate exit session calls.
    if (exit_session_) {
      std::move(exit_session_).Run();
    }
    return;
  }

  // Set initialization complete.
  waiting_for_initial_policy_fetch_ = false;

  // Publish policy (even though it hasn't changed) in order to signal load
  // complete on the ConfigurationPolicyProvider interface.
  PublishPolicy();
}

}  // namespace policy
