// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/profile_policy_connector.h"

#include <vector>

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/policy_service_impl.h"

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/policy/managed_mode_policy_provider.h"
#endif

#if defined(OS_CHROMEOS)
#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_provider.h"
#include "chrome/browser/chromeos/policy/network_configuration_updater.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_factory_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#else
#include "chrome/browser/policy/cloud/user_cloud_policy_manager.h"
#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"
#endif

namespace policy {

ProfilePolicyConnector::ProfilePolicyConnector(Profile* profile)
    : profile_(profile),
#if defined(OS_CHROMEOS)
      is_primary_user_(false),
#endif
      weak_ptr_factory_(this) {}

ProfilePolicyConnector::~ProfilePolicyConnector() {}

void ProfilePolicyConnector::Init(
    bool force_immediate_load,
    base::SequencedTaskRunner* sequenced_task_runner) {
  BrowserPolicyConnector* connector =
      g_browser_process->browser_policy_connector();
  // |providers| contains a list of the policy providers available for the
  // PolicyService of this connector.
  std::vector<ConfigurationPolicyProvider*> providers;

#if defined(OS_CHROMEOS)
  UserCloudPolicyManagerChromeOS* cloud_policy_manager =
      UserCloudPolicyManagerFactoryChromeOS::GetForProfile(profile_);
  if (cloud_policy_manager)
    providers.push_back(cloud_policy_manager);

  bool is_managed = false;
  std::string username;
  if (!chromeos::ProfileHelper::IsSigninProfile(profile_)) {
    // |user| should never be NULL except for the signin profile.
    // TODO(joaodasilva): get the |user| that corresponds to the |profile_|
    // from the ProfileHelper, once that's ready.
    chromeos::UserManager* user_manager = chromeos::UserManager::Get();
    chromeos::User* user = user_manager->GetActiveUser();
    CHECK(user);
    // Check if |user| is managed, and if it's a public account.
    username = user->email();
    is_managed =
        connector->GetUserAffiliation(username) == USER_AFFILIATION_MANAGED;
    is_primary_user_ =
        chromeos::UserManager::Get()->GetLoggedInUsers().size() == 1;
    if (user->GetType() == chromeos::User::USER_TYPE_PUBLIC_ACCOUNT)
      InitializeDeviceLocalAccountPolicyProvider(username);
    if (device_local_account_policy_provider_)
      providers.push_back(device_local_account_policy_provider_.get());
  }
#else
  UserCloudPolicyManager* cloud_policy_manager =
      UserCloudPolicyManagerFactory::GetForProfile(profile_);
  if (cloud_policy_manager)
    providers.push_back(cloud_policy_manager);
#endif

#if defined(ENABLE_MANAGED_USERS)
  managed_mode_policy_provider_ = ManagedModePolicyProvider::Create(
      profile_, sequenced_task_runner, force_immediate_load);
  managed_mode_policy_provider_->Init();
  providers.push_back(managed_mode_policy_provider_.get());
#endif

  policy_service_ = connector->CreatePolicyService(providers);

#if defined(OS_CHROMEOS)
  if (is_primary_user_) {
    if (cloud_policy_manager) {
      connector->SetUserPolicyDelegate(cloud_policy_manager);
    } else if (device_local_account_policy_provider_) {
      connector->SetUserPolicyDelegate(
          device_local_account_policy_provider_.get());
    }

    chromeos::CryptohomeClient* cryptohome_client =
        chromeos::DBusThreadManager::Get()->GetCryptohomeClient();
    cryptohome_client->GetSanitizedUsername(
        username,
        base::Bind(
            &ProfilePolicyConnector::InitializeNetworkConfigurationUpdater,
            weak_ptr_factory_.GetWeakPtr(),
            is_managed));
  }
#endif
}

void ProfilePolicyConnector::InitForTesting(scoped_ptr<PolicyService> service) {
  policy_service_ = service.Pass();
}

void ProfilePolicyConnector::Shutdown() {
#if defined(OS_CHROMEOS)
  if (is_primary_user_)
    g_browser_process->browser_policy_connector()->SetUserPolicyDelegate(NULL);
  if (device_local_account_policy_provider_)
    device_local_account_policy_provider_->Shutdown();
#endif

#if defined(ENABLE_MANAGED_USERS)
  if (managed_mode_policy_provider_)
    managed_mode_policy_provider_->Shutdown();
#endif
}

bool ProfilePolicyConnector::UsedPolicyCertificates() {
#if defined(OS_CHROMEOS)
  return profile_->GetPrefs()->GetBoolean(prefs::kUsedPolicyCertificatesOnce);
#else
  return false;
#endif
}

#if defined(OS_CHROMEOS)
void ProfilePolicyConnector::InitializeDeviceLocalAccountPolicyProvider(
    const std::string& username) {
  BrowserPolicyConnector* connector =
      g_browser_process->browser_policy_connector();
  DeviceLocalAccountPolicyService* device_local_account_policy_service =
      connector->GetDeviceLocalAccountPolicyService();
  if (!device_local_account_policy_service)
    return;
  device_local_account_policy_provider_.reset(
      new DeviceLocalAccountPolicyProvider(
          username, device_local_account_policy_service));
  device_local_account_policy_provider_->Init();
}

void ProfilePolicyConnector::InitializeNetworkConfigurationUpdater(
    bool is_managed,
    chromeos::DBusMethodCallStatus status,
    const std::string& hashed_username) {
  // TODO(joaodasilva): create the NetworkConfigurationUpdater for user ONC
  // here, after splitting that class into an instance for device policy and
  // another per profile for user policy.
  BrowserPolicyConnector* connector =
      g_browser_process->browser_policy_connector();
  NetworkConfigurationUpdater* network_updater =
      connector->GetNetworkConfigurationUpdater();
  network_updater->OnUserPolicyInitialized(is_managed, hashed_username);
}
#endif

}  // namespace policy
