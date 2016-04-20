// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/prefs/pref_service.h"
#include "net/url_request/url_request_context_getter.h"

#if !defined(OS_ANDROID) && !defined(OS_IOS)
#include "components/policy/core/common/cloud/resource_cache.h"
#endif

namespace policy {

CloudPolicyManager::CloudPolicyManager(
    const std::string& policy_type,
    const std::string& settings_entity_id,
    CloudPolicyStore* cloud_policy_store,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& file_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner)
    : core_(policy_type, settings_entity_id, cloud_policy_store, task_runner),
      waiting_for_policy_refresh_(false),
      file_task_runner_(file_task_runner),
      io_task_runner_(io_task_runner) {
  store()->AddObserver(this);

  // If the underlying store is already initialized, publish the loaded
  // policy. Otherwise, request a load now.
  if (store()->is_initialized())
    CheckAndPublishPolicy();
  else
    store()->Load();
}

CloudPolicyManager::~CloudPolicyManager() {}

void CloudPolicyManager::Shutdown() {
  component_policy_service_.reset();
  core_.Disconnect();
  store()->RemoveObserver(this);
  ConfigurationPolicyProvider::Shutdown();
}

bool CloudPolicyManager::IsInitializationComplete(PolicyDomain domain) const {
  if (domain == POLICY_DOMAIN_CHROME)
    return store()->is_initialized();
  if (ComponentCloudPolicyService::SupportsDomain(domain) &&
      component_policy_service_) {
    return component_policy_service_->is_initialized();
  }
  return true;
}

void CloudPolicyManager::RefreshPolicies() {
  if (service()) {
    waiting_for_policy_refresh_ = true;
    service()->RefreshPolicy(
        base::Bind(&CloudPolicyManager::OnRefreshComplete,
                   base::Unretained(this)));
  } else {
    OnRefreshComplete(false);
  }
}

void CloudPolicyManager::OnStoreLoaded(CloudPolicyStore* cloud_policy_store) {
  DCHECK_EQ(store(), cloud_policy_store);
  CheckAndPublishPolicy();
}

void CloudPolicyManager::OnStoreError(CloudPolicyStore* cloud_policy_store) {
  DCHECK_EQ(store(), cloud_policy_store);
  // Publish policy (even though it hasn't changed) in order to signal load
  // complete on the ConfigurationPolicyProvider interface. Technically, this
  // is only required on the first load, but doesn't hurt in any case.
  CheckAndPublishPolicy();
}

void CloudPolicyManager::OnComponentCloudPolicyUpdated() {
  CheckAndPublishPolicy();
}

void CloudPolicyManager::CheckAndPublishPolicy() {
  if (IsInitializationComplete(POLICY_DOMAIN_CHROME) &&
      !waiting_for_policy_refresh_) {
    std::unique_ptr<PolicyBundle> bundle(new PolicyBundle);
    GetChromePolicy(
        &bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string())));
    if (component_policy_service_)
      bundle->MergeFrom(component_policy_service_->policy());
    UpdatePolicy(std::move(bundle));
  }
}

void CloudPolicyManager::GetChromePolicy(PolicyMap* policy_map) {
  policy_map->CopyFrom(store()->policy_map());
}

void CloudPolicyManager::CreateComponentCloudPolicyService(
    const base::FilePath& policy_cache_path,
    const scoped_refptr<net::URLRequestContextGetter>& request_context,
    CloudPolicyClient* client) {
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  // Init() must have been called.
  CHECK(schema_registry());
  // Called at most once.
  CHECK(!component_policy_service_);
  // The core can't be connected yet.
  // See the comments on ComponentCloudPolicyService for the details.
  CHECK(!core()->client());

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableComponentCloudPolicy) ||
      policy_cache_path.empty()) {
    return;
  }

  // TODO(joaodasilva): Move the |file_task_runner_| to the blocking pool.
  // Currently it's not possible because the ComponentCloudPolicyStore is
  // NonThreadSafe and doesn't support getting calls from different threads.
  std::unique_ptr<ResourceCache> resource_cache(
      new ResourceCache(policy_cache_path, file_task_runner_));
  component_policy_service_.reset(new ComponentCloudPolicyService(
      this, schema_registry(), core(), client, std::move(resource_cache),
      request_context, file_task_runner_, io_task_runner_));
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)
}

void CloudPolicyManager::ClearAndDestroyComponentCloudPolicyService() {
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  if (component_policy_service_) {
    component_policy_service_->ClearCache();
    component_policy_service_.reset();
  }
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)
}

void CloudPolicyManager::OnRefreshComplete(bool success) {
  waiting_for_policy_refresh_ = false;
  CheckAndPublishPolicy();
}

}  // namespace policy
