// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/active_directory_policy_manager.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/dbus/auth_policy_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/tools/variable_expander.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/policy_constants.h"
#include "net/url_request/url_request_context_getter.h"

namespace em = enterprise_management;

namespace policy {
namespace {

// List of policies where variables like ${machine_name} should be expanded.
constexpr const char* kPoliciesToExpand[] = {key::kNativePrinters};

// Fetch policy every 90 minutes which matches the Windows default:
// https://technet.microsoft.com/en-us/library/cc940895.aspx
constexpr base::TimeDelta kFetchInterval = base::TimeDelta::FromMinutes(90);

void RunRefreshCallback(base::OnceCallback<void(bool success)> callback,
                        authpolicy::ErrorType error) {
  std::move(callback).Run(error == authpolicy::ERROR_NONE);
}

// Gets the AuthPolicy D-Bus interface.
chromeos::AuthPolicyClient* GetAuthPolicyClient() {
  chromeos::DBusThreadManager* thread_manager =
      chromeos::DBusThreadManager::Get();
  DCHECK(thread_manager);
  chromeos::AuthPolicyClient* auth_policy_client =
      thread_manager->GetAuthPolicyClient();
  DCHECK(auth_policy_client);
  return auth_policy_client;
}

}  // namespace

ActiveDirectoryPolicyManager::~ActiveDirectoryPolicyManager() = default;

void ActiveDirectoryPolicyManager::Init(SchemaRegistry* registry) {
  ConfigurationPolicyProvider::Init(registry);

  store_->AddObserver(this);
  if (!store_->is_initialized()) {
    store_->Load();
  }

  // Does nothing if |store_| hasn't yet initialized.
  PublishPolicy();

  scheduler_ = std::make_unique<PolicyScheduler>(
      base::BindRepeating(&ActiveDirectoryPolicyManager::DoPolicyFetch,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&ActiveDirectoryPolicyManager::OnPolicyFetched,
                          weak_ptr_factory_.GetWeakPtr()),
      kFetchInterval);

  if (external_data_manager_) {
    // Use the system request context here instead of a context derived from the
    // Profile because Connect() is called before the profile is fully
    // initialized (required so we can perform the initial policy load).
    // Note: The request context can be null for tests and for device policy.
    external_data_manager_->Connect(
        g_browser_process->system_request_context());
  }
}

void ActiveDirectoryPolicyManager::Shutdown() {
  if (external_data_manager_)
    external_data_manager_->Disconnect();
  store_->RemoveObserver(this);
  ConfigurationPolicyProvider::Shutdown();
}

bool ActiveDirectoryPolicyManager::IsInitializationComplete(
    PolicyDomain domain) const {
  if (domain == POLICY_DOMAIN_CHROME)
    return store_->is_initialized();
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

ActiveDirectoryPolicyManager::ActiveDirectoryPolicyManager(
    std::unique_ptr<CloudPolicyStore> store,
    std::unique_ptr<CloudExternalDataManager> external_data_manager)
    : store_(std::move(store)),
      external_data_manager_(std::move(external_data_manager)) {}

void ActiveDirectoryPolicyManager::PublishPolicy() {
  if (!store_->is_initialized()) {
    return;
  }
  std::unique_ptr<PolicyBundle> bundle = std::make_unique<PolicyBundle>();
  PolicyMap& policy_map =
      bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  policy_map.CopyFrom(store_->policy_map());

  // Overwrite the source which is POLICY_SOURCE_CLOUD by default.
  // TODO(tnagel): Rename CloudPolicyStore to PolicyStore and make the source
  // configurable, then drop PolicyMap::SetSourceForAll().
  policy_map.SetSourceForAll(POLICY_SOURCE_ACTIVE_DIRECTORY);
  SetEnterpriseUsersDefaults(&policy_map);

  // Expand e.g. ${machine_name} for a selected set of policies.
  ExpandVariables(&policy_map);

  // Policy is ready, send it off.
  UpdatePolicy(std::move(bundle));
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

void ActiveDirectoryPolicyManager::ExpandVariables(PolicyMap* policy_map) {
  const em::PolicyData* policy = store_->policy();
  if (!policy)
    return;
  if (policy->machine_name().empty()) {
    LOG(ERROR) << "Cannot expand machine_name (empty string in policy)";
    return;
  }

  chromeos::VariableExpander expander(
      {{"machine_name", policy->machine_name()}});
  for (const char* policy_name : kPoliciesToExpand) {
    base::Value* value = policy_map->GetMutableValue(policy_name);
    if (value) {
      if (!expander.ExpandValue(value)) {
        LOG(ERROR) << "Failed to expand at least one variable in policy "
                   << policy_name;
      }
    }
  }
}

UserActiveDirectoryPolicyManager::UserActiveDirectoryPolicyManager(
    const AccountId& account_id,
    base::TimeDelta initial_policy_fetch_timeout,
    base::OnceClosure exit_session,
    std::unique_ptr<CloudPolicyStore> store,
    std::unique_ptr<CloudExternalDataManager> external_data_manager)
    : ActiveDirectoryPolicyManager(std::move(store),
                                   std::move(external_data_manager)),
      account_id_(account_id),
      waiting_for_initial_policy_fetch_(
          !initial_policy_fetch_timeout.is_zero()),
      initial_policy_fetch_may_fail_(!initial_policy_fetch_timeout.is_max()),
      exit_session_(std::move(exit_session)) {
  // Delaying initialization complete is intended for user policy only.
  if (waiting_for_initial_policy_fetch_ && initial_policy_fetch_may_fail_) {
    initial_policy_timeout_.Start(
        FROM_HERE, initial_policy_fetch_timeout,
        base::Bind(&UserActiveDirectoryPolicyManager::OnBlockingFetchTimeout,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

UserActiveDirectoryPolicyManager::~UserActiveDirectoryPolicyManager() = default;

bool UserActiveDirectoryPolicyManager::IsInitializationComplete(
    PolicyDomain domain) const {
  if (waiting_for_initial_policy_fetch_)
    return false;

  return ActiveDirectoryPolicyManager::IsInitializationComplete(domain);
}

void UserActiveDirectoryPolicyManager::ForceTimeoutForTesting() {
  DCHECK(initial_policy_timeout_.IsRunning());
  // Stop the timer to mimic what happens when a real timer fires, then invoke
  // the timer callback directly.
  initial_policy_timeout_.Stop();
  OnBlockingFetchTimeout();
}

void UserActiveDirectoryPolicyManager::DoPolicyFetch(
    PolicyScheduler::TaskCallback callback) {
  GetAuthPolicyClient()->RefreshUserPolicy(
      account_id_, base::BindOnce(&RunRefreshCallback, std::move(callback)));
}

void UserActiveDirectoryPolicyManager::CancelWaitForInitialPolicy(
    bool success) {
  if (!waiting_for_initial_policy_fetch_)
    return;

  initial_policy_timeout_.Stop();

  // If the conditions to continue profile initialization are not met, the user
  // session is exited and initialization is not set as completed.
  // TODO(tnagel): Maybe add code to retry policy fetch?
  if (!store()->has_policy()) {
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

void UserActiveDirectoryPolicyManager::OnBlockingFetchTimeout() {
  DCHECK(waiting_for_initial_policy_fetch_);
  LOG(WARNING) << "Timed out while waiting for the policy fetch. "
               << "The session will start with the cached policy.";
  CancelWaitForInitialPolicy(false /* success */);
}

DeviceActiveDirectoryPolicyManager::DeviceActiveDirectoryPolicyManager(
    std::unique_ptr<CloudPolicyStore> store)
    : ActiveDirectoryPolicyManager(std::move(store),
                                   nullptr /* external_data_manager */) {}

DeviceActiveDirectoryPolicyManager::~DeviceActiveDirectoryPolicyManager() =
    default;

void DeviceActiveDirectoryPolicyManager::DoPolicyFetch(
    base::OnceCallback<void(bool success)> callback) {
  GetAuthPolicyClient()->RefreshDevicePolicy(
      base::BindOnce(&RunRefreshCallback, std::move(callback)));
}

}  // namespace policy
