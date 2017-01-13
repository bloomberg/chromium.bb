// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_active_directory_policy_manager.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chromeos/dbus/auth_policy_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_types.h"

namespace policy {

DeviceActiveDirectoryPolicyManager::DeviceActiveDirectoryPolicyManager(
    std::unique_ptr<CloudPolicyStore> store)
    : store_(std::move(store)), weak_ptr_factory_(this) {}

DeviceActiveDirectoryPolicyManager::~DeviceActiveDirectoryPolicyManager() {}

void DeviceActiveDirectoryPolicyManager::Init(SchemaRegistry* registry) {
  ConfigurationPolicyProvider::Init(registry);

  store_->AddObserver(this);
  if (!store_->is_initialized()) {
    store_->Load();
  }

  // Does nothing if |store_| hasn't yet initialized.
  PublishPolicy();

  RefreshPolicies();
}

void DeviceActiveDirectoryPolicyManager::Shutdown() {
  store_->RemoveObserver(this);
  ConfigurationPolicyProvider::Shutdown();
}

bool DeviceActiveDirectoryPolicyManager::IsInitializationComplete(
    PolicyDomain domain) const {
  if (domain == POLICY_DOMAIN_CHROME)
    return store_->is_initialized();
  return true;
}

void DeviceActiveDirectoryPolicyManager::RefreshPolicies() {
  chromeos::DBusThreadManager* thread_manager =
      chromeos::DBusThreadManager::Get();
  DCHECK(thread_manager);
  chromeos::AuthPolicyClient* auth_policy_client =
      thread_manager->GetAuthPolicyClient();
  DCHECK(auth_policy_client);
  auth_policy_client->RefreshDevicePolicy(
      base::Bind(&DeviceActiveDirectoryPolicyManager::OnPolicyRefreshed,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DeviceActiveDirectoryPolicyManager::OnStoreLoaded(
    CloudPolicyStore* cloud_policy_store) {
  DCHECK_EQ(store_.get(), cloud_policy_store);
  PublishPolicy();
}

void DeviceActiveDirectoryPolicyManager::OnStoreError(
    CloudPolicyStore* cloud_policy_store) {
  DCHECK_EQ(store_.get(), cloud_policy_store);
  // Publish policy (even though it hasn't changed) in order to signal load
  // complete on the ConfigurationPolicyProvider interface. Technically, this is
  // only required on the first load, but doesn't hurt in any case.
  PublishPolicy();
}

void DeviceActiveDirectoryPolicyManager::PublishPolicy() {
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
  UpdatePolicy(std::move(bundle));
}

void DeviceActiveDirectoryPolicyManager::OnPolicyRefreshed(bool success) {
  if (!success) {
    LOG(ERROR) << "Active Directory policy refresh failed.";
  }
  // Load independently of success or failure to keep up to date with whatever
  // has happened on the authpolicyd / session manager side.
  store_->Load();
}

}  // namespace policy
