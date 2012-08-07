// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/user_cloud_policy_manager.h"

#include <string>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "chrome/browser/policy/cloud_policy_refresh_scheduler.h"
#include "chrome/browser/policy/cloud_policy_service.h"
#include "chrome/browser/policy/policy_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/policy/user_cloud_policy_store_chromeos.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#endif

namespace policy {

namespace {

#if defined(OS_CHROMEOS)
// Paths for the legacy policy caches in the profile directory.
// TODO(mnissler): Remove once the number of pre-M20 clients becomes negligible.

// Subdirectory in the user's profile for storing user policies.
const FilePath::CharType kPolicyDir[] = FILE_PATH_LITERAL("Device Management");
// File in the above directory for stroing user policy dmtokens.
const FilePath::CharType kTokenCacheFile[] = FILE_PATH_LITERAL("Token");
// File in the above directory for storing user policy data.
const FilePath::CharType kPolicyCacheFile[] = FILE_PATH_LITERAL("Policy");
#endif

}  // namespace

UserCloudPolicyManager::UserCloudPolicyManager(
    scoped_ptr<CloudPolicyStore> store,
    bool wait_for_policy_fetch)
    : wait_for_policy_fetch_(wait_for_policy_fetch),
      wait_for_policy_refresh_(false),
      store_(store.Pass()) {
  store_->Load();
  store_->AddObserver(this);
}

UserCloudPolicyManager::~UserCloudPolicyManager() {
  Shutdown();
  store_->RemoveObserver(this);
}

#if defined(OS_CHROMEOS)
// static
scoped_ptr<UserCloudPolicyManager> UserCloudPolicyManager::Create(
    bool wait_for_policy_fetch) {
  FilePath profile_dir;
  CHECK(PathService::Get(chrome::DIR_USER_DATA, &profile_dir));
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  const FilePath policy_dir =
      profile_dir
          .Append(command_line->GetSwitchValuePath(switches::kLoginProfile))
          .Append(kPolicyDir);
  const FilePath policy_cache_file = policy_dir.Append(kPolicyCacheFile);
  const FilePath token_cache_file = policy_dir.Append(kTokenCacheFile);

  scoped_ptr<CloudPolicyStore> store(
      new UserCloudPolicyStoreChromeOS(
          chromeos::DBusThreadManager::Get()->GetSessionManagerClient(),
          token_cache_file, policy_cache_file));
  return scoped_ptr<UserCloudPolicyManager>(
      new UserCloudPolicyManager(store.Pass(), wait_for_policy_fetch));
}
#endif

void UserCloudPolicyManager::Initialize(PrefService* prefs,
                                        DeviceManagementService* service,
                                        UserAffiliation user_affiliation) {
  DCHECK(!service_.get());
  prefs_ = prefs;
  scoped_ptr<CloudPolicyClient> client(
      new CloudPolicyClient(std::string(), std::string(), user_affiliation,
                            POLICY_SCOPE_USER, NULL, service));
  service_.reset(new CloudPolicyService(client.Pass(), store_.get()));
  service_->client()->AddObserver(this);

  if (wait_for_policy_fetch_) {
    // If we are supposed to wait for a policy fetch, we trigger an explicit
    // policy refresh at startup that allows us to unblock initialization once
    // done. The refresh scheduler only gets started once that refresh
    // completes. Note that we might have to wait for registration to happen,
    // see OnRegistrationStateChanged() below.
    if (service_->client()->is_registered()) {
      service_->RefreshPolicy(
          base::Bind(&UserCloudPolicyManager::OnInitialPolicyFetchComplete,
                     base::Unretained(this)));
    }
  } else {
    CancelWaitForPolicyFetch();
  }
}

void UserCloudPolicyManager::Shutdown() {
  refresh_scheduler_.reset();
  if (service_.get())
    service_->client()->RemoveObserver(this);
  service_.reset();
  prefs_ = NULL;
}

void UserCloudPolicyManager::CancelWaitForPolicyFetch() {
  wait_for_policy_fetch_ = false;
  CheckAndPublishPolicy();

  // Now that |wait_for_policy_fetch_| is guaranteed to be false, the scheduler
  // can be started.
  if (service_.get() && !refresh_scheduler_.get() && prefs_) {
    refresh_scheduler_.reset(
        new CloudPolicyRefreshScheduler(
            service_->client(), store_.get(), prefs_,
            prefs::kUserPolicyRefreshRate,
            MessageLoop::current()->message_loop_proxy()));
  }
}

bool UserCloudPolicyManager::IsInitializationComplete() const {
  return store_->is_initialized() && !wait_for_policy_fetch_;
}

void UserCloudPolicyManager::RefreshPolicies() {
  if (service_.get()) {
    wait_for_policy_refresh_ = true;
    service_->RefreshPolicy(
        base::Bind(&UserCloudPolicyManager::OnRefreshComplete,
                   base::Unretained(this)));
  } else {
    OnRefreshComplete();
  }
}

void UserCloudPolicyManager::OnPolicyFetched(CloudPolicyClient* client) {
  // No action required. If we're blocked on a policy fetch, we'll learn about
  // completion of it through OnInitialPolicyFetchComplete().
}

void UserCloudPolicyManager::OnRegistrationStateChanged(
    CloudPolicyClient* client) {
  if (wait_for_policy_fetch_) {
    // If we're blocked on the policy fetch, now is a good time to issue it.
    if (service_->client()->is_registered()) {
      service_->RefreshPolicy(
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
  CancelWaitForPolicyFetch();
}

void UserCloudPolicyManager::CheckAndPublishPolicy() {
  if (IsInitializationComplete() && !wait_for_policy_refresh_) {
    scoped_ptr<PolicyBundle> bundle(new PolicyBundle());
    bundle->Get(POLICY_DOMAIN_CHROME, std::string()).CopyFrom(
        store_->policy_map());
    UpdatePolicy(bundle.Pass());
  }
}

void UserCloudPolicyManager::OnStoreLoaded(CloudPolicyStore* store) {
  CheckAndPublishPolicy();
}

void UserCloudPolicyManager::OnStoreError(CloudPolicyStore* store) {
  // No action required, the old policy is still valid.
}

void UserCloudPolicyManager::OnInitialPolicyFetchComplete() {
  CancelWaitForPolicyFetch();
}

void UserCloudPolicyManager::OnRefreshComplete() {
  wait_for_policy_refresh_ = false;
  CheckAndPublishPolicy();
}

}  // namespace policy
