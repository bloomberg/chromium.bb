// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/policy/core/common/cloud/user_cloud_policy_store.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "net/url_request/url_request_context_getter.h"

namespace em = enterprise_management;

namespace policy {

UserCloudPolicyManager::UserCloudPolicyManager(
    std::unique_ptr<UserCloudPolicyStore> store,
    const base::FilePath& component_policy_cache_path,
    std::unique_ptr<CloudExternalDataManager> external_data_manager,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& file_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner)
    : CloudPolicyManager(dm_protocol::kChromeUserPolicyType,
                         std::string(),
                         store.get(),
                         task_runner,
                         file_task_runner,
                         io_task_runner),
      store_(std::move(store)),
      component_policy_cache_path_(component_policy_cache_path),
      external_data_manager_(std::move(external_data_manager)) {}

UserCloudPolicyManager::~UserCloudPolicyManager() {}

void UserCloudPolicyManager::Shutdown() {
  if (external_data_manager_)
    external_data_manager_->Disconnect();
  CloudPolicyManager::Shutdown();
}

void UserCloudPolicyManager::SetSigninUsername(const std::string& username) {
  store_->SetSigninUsername(username);
}

void UserCloudPolicyManager::Connect(
    PrefService* local_state,
    scoped_refptr<net::URLRequestContextGetter> request_context,
    std::unique_ptr<CloudPolicyClient> client) {
  CreateComponentCloudPolicyService(
      dm_protocol::kChromeExtensionPolicyType, component_policy_cache_path_,
      request_context, client.get(), schema_registry());
  core()->Connect(std::move(client));
  core()->StartRefreshScheduler();
  core()->TrackRefreshDelayPref(local_state,
                                policy_prefs::kUserPolicyRefreshRate);
  if (external_data_manager_)
    external_data_manager_->Connect(request_context);
}

// static
std::unique_ptr<CloudPolicyClient>
UserCloudPolicyManager::CreateCloudPolicyClient(
    DeviceManagementService* device_management_service,
    scoped_refptr<net::URLRequestContextGetter> request_context) {
  return base::MakeUnique<CloudPolicyClient>(
      std::string() /* machine_id */, std::string() /* machine_model */,
      device_management_service, request_context,
      nullptr /* signing_service */);
}

void UserCloudPolicyManager::DisconnectAndRemovePolicy() {
  if (external_data_manager_)
    external_data_manager_->Disconnect();
  core()->Disconnect();

  // store_->Clear() will publish the updated, empty policy. The component
  // policy service must be cleared before OnStoreLoaded() is issued, so that
  // component policies are also empty at CheckAndPublishPolicy().
  ClearAndDestroyComponentCloudPolicyService();

  // When the |store_| is cleared, it informs the |external_data_manager_| that
  // all external data references have been removed, causing the
  // |external_data_manager_| to clear its cache as well.
  store_->Clear();
}

bool UserCloudPolicyManager::IsClientRegistered() const {
  return client() && client()->is_registered();
}

void UserCloudPolicyManager::GetChromePolicy(PolicyMap* policy_map) {
  CloudPolicyManager::GetChromePolicy(policy_map);

  // If the store has a verified policy blob received from the server then apply
  // the defaults for policies that haven't been configured by the administrator
  // given that this is an enterprise user.
  // TODO(treib,atwilson): We should just call SetEnterpriseUsersDefaults here,
  // see crbug.com/640950.
  if (store()->has_policy() &&
      !policy_map->Get(key::kNTPContentSuggestionsEnabled)) {
    policy_map->Set(key::kNTPContentSuggestionsEnabled, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_USER, POLICY_SOURCE_ENTERPRISE_DEFAULT,
                    base::MakeUnique<base::Value>(false),
                    nullptr /* external_data_fetcher */);
  }
}

}  // namespace policy
