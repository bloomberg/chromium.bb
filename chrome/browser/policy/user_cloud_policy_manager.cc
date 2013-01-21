// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/user_cloud_policy_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "chrome/browser/policy/cloud_policy_service.h"
#include "chrome/browser/policy/policy_types.h"
#include "chrome/browser/policy/user_cloud_policy_manager_factory.h"
#include "chrome/browser/policy/user_cloud_policy_store.h"
#include "chrome/common/pref_names.h"

namespace em = enterprise_management;

namespace policy {

UserCloudPolicyManager::UserCloudPolicyManager(
    Profile* profile,
    scoped_ptr<UserCloudPolicyStore> store)
    : CloudPolicyManager(
          PolicyNamespaceKey(dm_protocol::kChromeUserPolicyType, std::string()),
          store.get()),
      profile_(profile),
      store_(store.Pass()) {
  UserCloudPolicyManagerFactory::GetInstance()->Register(profile_, this);
}

UserCloudPolicyManager::~UserCloudPolicyManager() {
  UserCloudPolicyManagerFactory::GetInstance()->Unregister(profile_, this);
}

void UserCloudPolicyManager::Connect(
    PrefService* local_state,
    DeviceManagementService* device_management_service) {
  core()->Connect(
      make_scoped_ptr(new CloudPolicyClient(std::string(), std::string(),
                                            USER_AFFILIATION_NONE,
                                            NULL, device_management_service)));
  core()->StartRefreshScheduler();
  core()->TrackRefreshDelayPref(local_state, prefs::kUserPolicyRefreshRate);
}

void UserCloudPolicyManager::DisconnectAndRemovePolicy() {
  core()->Disconnect();
  store_->Clear();
}

bool UserCloudPolicyManager::IsClientRegistered() const {
  return client() && client()->is_registered();
}

void UserCloudPolicyManager::RegisterClient(const std::string& access_token) {
  DCHECK(client()) << "Callers must invoke Initialize() first";
  if (!client()->is_registered()) {
    DVLOG(1) << "Registering client with access token: " << access_token;
    client()->Register(em::DeviceRegisterRequest::USER,
                       access_token, std::string(), false);
  }
}

}  // namespace policy
