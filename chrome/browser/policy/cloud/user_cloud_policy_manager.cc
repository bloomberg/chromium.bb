// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_cloud_policy_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/policy/cloud/cloud_external_data_manager.h"
#include "chrome/browser/policy/cloud/cloud_policy_constants.h"
#include "chrome/browser/policy/cloud/cloud_policy_service.h"
#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"
#include "chrome/browser/policy/cloud/user_cloud_policy_store.h"
#include "chrome/browser/policy/policy_types.h"
#include "chrome/common/pref_names.h"
#include "net/url_request/url_request_context_getter.h"

namespace em = enterprise_management;

namespace policy {

UserCloudPolicyManager::UserCloudPolicyManager(
    Profile* profile,
    scoped_ptr<UserCloudPolicyStore> store,
    scoped_ptr<CloudExternalDataManager> external_data_manager,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : CloudPolicyManager(
          PolicyNamespaceKey(GetChromeUserPolicyType(), std::string()),
          store.get(),
          task_runner),
      profile_(profile),
      store_(store.Pass()),
      external_data_manager_(external_data_manager.Pass()) {
  UserCloudPolicyManagerFactory::GetInstance()->Register(profile_, this);
}

UserCloudPolicyManager::~UserCloudPolicyManager() {
  UserCloudPolicyManagerFactory::GetInstance()->Unregister(profile_, this);
}

void UserCloudPolicyManager::Shutdown() {
  if (external_data_manager_)
    external_data_manager_->Disconnect();
  CloudPolicyManager::Shutdown();
  BrowserContextKeyedService::Shutdown();
}

void UserCloudPolicyManager::Connect(
    PrefService* local_state,
    scoped_refptr<net::URLRequestContextGetter> request_context,
    scoped_ptr<CloudPolicyClient> client) {
  core()->Connect(client.Pass());
  core()->StartRefreshScheduler();
  core()->TrackRefreshDelayPref(local_state, prefs::kUserPolicyRefreshRate);
  if (external_data_manager_)
    external_data_manager_->Connect(request_context);
}

// static
scoped_ptr<CloudPolicyClient>
UserCloudPolicyManager::CreateCloudPolicyClient(
    DeviceManagementService* device_management_service) {
  return make_scoped_ptr(
      new CloudPolicyClient(std::string(), std::string(),
                            USER_AFFILIATION_NONE,
                            NULL, device_management_service)).Pass();
}

void UserCloudPolicyManager::DisconnectAndRemovePolicy() {
  if (external_data_manager_)
    external_data_manager_->Disconnect();
  core()->Disconnect();
  // When the |store_| is cleared, it informs the |external_data_manager_| that
  // all external data references have been removed, causing the
  // |external_data_manager_| to clear its cache as well.
  store_->Clear();
}

bool UserCloudPolicyManager::IsClientRegistered() const {
  return client() && client()->is_registered();
}

}  // namespace policy
