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

// Refresh policy every 90 minutes which matches the Windows default:
// https://technet.microsoft.com/en-us/library/cc940895.aspx
constexpr base::TimeDelta kRefreshInterval = base::TimeDelta::FromMinutes(90);

}  // namespace

namespace policy {

ActiveDirectoryPolicyManager::~ActiveDirectoryPolicyManager() {}

// static
std::unique_ptr<ActiveDirectoryPolicyManager>
ActiveDirectoryPolicyManager::CreateForDevicePolicy(
    std::unique_ptr<CloudPolicyStore> store) {
  return base::WrapUnique(
      new ActiveDirectoryPolicyManager(EmptyAccountId(), std::move(store)));
}

// static
std::unique_ptr<ActiveDirectoryPolicyManager>
ActiveDirectoryPolicyManager::CreateForUserPolicy(
    const AccountId& account_id,
    std::unique_ptr<CloudPolicyStore> store) {
  return base::WrapUnique(
      new ActiveDirectoryPolicyManager(account_id, std::move(store)));
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
      base::BindRepeating(&ActiveDirectoryPolicyManager::DoRefresh,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&ActiveDirectoryPolicyManager::OnPolicyRefreshed,
                          weak_ptr_factory_.GetWeakPtr()),
      kRefreshInterval);
}

void ActiveDirectoryPolicyManager::Shutdown() {
  store_->RemoveObserver(this);
  ConfigurationPolicyProvider::Shutdown();
}

bool ActiveDirectoryPolicyManager::IsInitializationComplete(
    PolicyDomain domain) const {
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
}

void ActiveDirectoryPolicyManager::OnStoreError(
    CloudPolicyStore* cloud_policy_store) {
  DCHECK_EQ(store_.get(), cloud_policy_store);
  // Publish policy (even though it hasn't changed) in order to signal load
  // complete on the ConfigurationPolicyProvider interface. Technically, this is
  // only required on the first load, but doesn't hurt in any case.
  PublishPolicy();
}

ActiveDirectoryPolicyManager::ActiveDirectoryPolicyManager(
    const AccountId& account_id,
    std::unique_ptr<CloudPolicyStore> store)
    : account_id_(account_id), store_(std::move(store)) {}

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

void ActiveDirectoryPolicyManager::DoRefresh(
    base::OnceCallback<void(bool success)> callback) {
  chromeos::DBusThreadManager* thread_manager =
      chromeos::DBusThreadManager::Get();
  DCHECK(thread_manager);
  chromeos::AuthPolicyClient* auth_policy_client =
      thread_manager->GetAuthPolicyClient();
  DCHECK(auth_policy_client);
  if (account_id_ == EmptyAccountId()) {
    auth_policy_client->RefreshDevicePolicy(std::move(callback));
  } else {
    auth_policy_client->RefreshUserPolicy(account_id_, std::move(callback));
  }
}

void ActiveDirectoryPolicyManager::OnPolicyRefreshed(bool success) {
  if (!success) {
    LOG(ERROR) << "Active Directory policy refresh failed.";
  }
  // Load independently of success or failure to keep up to date with whatever
  // has happened on the authpolicyd / session manager side.
  store_->Load();
}

}  // namespace policy
