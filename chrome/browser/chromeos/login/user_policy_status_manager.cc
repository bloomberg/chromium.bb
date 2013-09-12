// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_policy_status_manager.h"

#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_factory_chromeos.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/cloud/cloud_policy_store.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"

namespace chromeos {

namespace {

// A dictionary that maps user name to user policy status.
const char kUserPolicyStatus[] = "UserPolicyStatus";

}  // namespace

class UserPolicyStatusManager::StoreObserver
    : public policy::CloudPolicyStore::Observer {
 public:
  StoreObserver(UserPolicyStatusManager* manager,
                const std::string& user_email,
                Profile* profile)
      : manager_(manager),
        user_email_(user_email),
        store_(NULL) {
    policy::UserCloudPolicyManagerChromeOS* user_cloud_policy_manager =
        policy::UserCloudPolicyManagerFactoryChromeOS::GetForProfile(profile);
    if (user_cloud_policy_manager) {
      store_ = user_cloud_policy_manager->core()->store();
      store_->AddObserver(this);
      UpdateStatusFromStore();
    }
  }

  virtual ~StoreObserver() {
    if (store_)
      store_->RemoveObserver(this);
  }

 private:
  void UpdateStatusFromStore() {
    UserPolicyStatusManager::Set(user_email_,
                                 store_->is_managed()
                                     ? USER_POLICY_STATUS_MANAGED
                                     : USER_POLICY_STATUS_NONE);
    if (store_->is_managed())
      manager_->CheckUserPolicyAllowed(user_email_);
  }

  // policy::CloudPolicyStore::Observer implementation.
  virtual void OnStoreLoaded(policy::CloudPolicyStore* store) OVERRIDE {
    DCHECK_EQ(store_, store);
    UpdateStatusFromStore();
  }

  virtual void OnStoreError(policy::CloudPolicyStore* store) OVERRIDE {
    DCHECK_EQ(store_, store);
    // Do nothing.
  }

  UserPolicyStatusManager* manager_;
  std::string user_email_;
  policy::CloudPolicyStore* store_;

  DISALLOW_COPY_AND_ASSIGN(StoreObserver);
};

UserPolicyStatusManager::UserPolicyStatusManager() {}
UserPolicyStatusManager::~UserPolicyStatusManager() {}

// static
void UserPolicyStatusManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kUserPolicyStatus);
}

// static
UserPolicyStatusManager::Status UserPolicyStatusManager::Get(
    const std::string& user_email) {
  PrefService* local_state = g_browser_process->local_state();
  const DictionaryValue* user_policy_status_dict =
      local_state->GetDictionary(kUserPolicyStatus);
  int status = USER_POLICY_STATUS_UNKNOWN;
  if (user_policy_status_dict &&
      user_policy_status_dict->GetIntegerWithoutPathExpansion(user_email,
                                                              &status)) {
    return static_cast<Status>(status);
  }

  return USER_POLICY_STATUS_UNKNOWN;
}

// static
void UserPolicyStatusManager::Set(const std::string& user_email,
                                  Status status) {
  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate update(local_state, kUserPolicyStatus);
  update->SetWithoutPathExpansion(
      user_email, new base::FundamentalValue(static_cast<int>(status)));
}

void UserPolicyStatusManager::StartObserving(const std::string& user_email,
                                             Profile* user_profile) {
  if (primary_user_email_.empty())
    primary_user_email_ = user_email;

  store_observers_.push_back(new StoreObserver(this, user_email, user_profile));
}

void UserPolicyStatusManager::CheckUserPolicyAllowed(
    const std::string& user_email) {
  // Only the primary user is allowed to have a policy.
  if (user_email == primary_user_email_)
    return;

  // Otherwise, shutdown the current session.
  LOG(ERROR)
      << "Shutdown session because a user with policy is not the primary user.";
  chrome::AttemptUserExit();
}

}  // namespace chromeos
