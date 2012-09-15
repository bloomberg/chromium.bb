// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/user_cloud_policy_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/policy/cloud_policy_service.h"
#include "chrome/browser/policy/policy_types.h"
#include "chrome/common/pref_names.h"

namespace policy {

UserCloudPolicyManager::UserCloudPolicyManager(
    scoped_ptr<CloudPolicyStore> store,
    bool wait_for_policy_fetch)
    : CloudPolicyManager(store.Pass()),
      wait_for_policy_fetch_(wait_for_policy_fetch) {}

UserCloudPolicyManager::~UserCloudPolicyManager() {
  if (cloud_policy_client())
    cloud_policy_client()->RemoveObserver(this);
}

// static
scoped_ptr<UserCloudPolicyManager> UserCloudPolicyManager::Create(
    Profile* profile,
    bool wait_for_policy_fetch) {
  scoped_ptr<CloudPolicyStore> store =
      CloudPolicyStore::CreateUserPolicyStore(profile);
  return scoped_ptr<UserCloudPolicyManager>(
      new UserCloudPolicyManager(store.Pass(), wait_for_policy_fetch));
}

void UserCloudPolicyManager::Initialize(
    PrefService* local_state,
    DeviceManagementService* device_management_service,
    UserAffiliation user_affiliation) {
  DCHECK(device_management_service);
  DCHECK(local_state);
  local_state_ = local_state;
  scoped_ptr<CloudPolicyClient> client(
      new CloudPolicyClient(std::string(), std::string(), user_affiliation,
                            POLICY_SCOPE_USER, NULL,
                            device_management_service));
  InitializeService(client.Pass());
  cloud_policy_client()->AddObserver(this);

  if (wait_for_policy_fetch_) {
    // If we are supposed to wait for a policy fetch, we trigger an explicit
    // policy refresh at startup that allows us to unblock initialization once
    // done. The refresh scheduler only gets started once that refresh
    // completes. Note that we might have to wait for registration to happen,
    // see OnRegistrationStateChanged() below.
    if (cloud_policy_client()->is_registered()) {
      cloud_policy_service()->RefreshPolicy(
          base::Bind(&UserCloudPolicyManager::OnInitialPolicyFetchComplete,
                     base::Unretained(this)));
    }
  } else {
    CancelWaitForPolicyFetch();
  }
}

void UserCloudPolicyManager::ShutdownAndRemovePolicy() {
  if (cloud_policy_client())
    cloud_policy_client()->RemoveObserver(this);
  ShutdownService();
  local_state_ = NULL;
  cloud_policy_store()->Clear();
}

void UserCloudPolicyManager::CancelWaitForPolicyFetch() {
  wait_for_policy_fetch_ = false;
  CheckAndPublishPolicy();

  // Now that |wait_for_policy_fetch_| is guaranteed to be false, the scheduler
  // can be started.
  if (cloud_policy_service() && local_state_)
    StartRefreshScheduler(local_state_, prefs::kUserPolicyRefreshRate);
}

bool UserCloudPolicyManager::IsClientRegistered() const {
  return cloud_policy_client() && cloud_policy_client()->is_registered();
}

void UserCloudPolicyManager::RegisterClient(const std::string& access_token) {
  DCHECK(cloud_policy_client()) << "Callers must invoke Initialize() first";
  if (!cloud_policy_client()->is_registered()) {
    DVLOG(1) << "Registering client with access token: " << access_token;
    cloud_policy_client()->Register(access_token);
  }
}

bool UserCloudPolicyManager::IsInitializationComplete() const {
  return CloudPolicyManager::IsInitializationComplete() &&
      !wait_for_policy_fetch_;
}

void UserCloudPolicyManager::OnPolicyFetched(CloudPolicyClient* client) {
  // No action required. If we're blocked on a policy fetch, we'll learn about
  // completion of it through OnInitialPolicyFetchComplete().
}

void UserCloudPolicyManager::OnRegistrationStateChanged(
    CloudPolicyClient* client) {
  DCHECK_EQ(cloud_policy_client(), client);
  if (wait_for_policy_fetch_) {
    // If we're blocked on the policy fetch, now is a good time to issue it.
    if (cloud_policy_client()->is_registered()) {
      cloud_policy_service()->RefreshPolicy(
          base::Bind(&UserCloudPolicyManager::OnInitialPolicyFetchComplete,
                     base::Unretained(this)));
    } else {
      // If the client has switched to not registered, we bail out as this
      // indicates the cloud policy setup flow has been aborted.
      CancelWaitForPolicyFetch();
    }
  }
}

void UserCloudPolicyManager::OnClientError(CloudPolicyClient* client) {
  DCHECK_EQ(cloud_policy_client(), client);
  CancelWaitForPolicyFetch();
}

void UserCloudPolicyManager::OnInitialPolicyFetchComplete() {
  CancelWaitForPolicyFetch();
}

}  // namespace policy
