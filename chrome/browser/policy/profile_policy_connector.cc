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
#include "chrome/browser/chromeos/policy/login_profile_policy_provider.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_factory_chromeos.h"
#include "chrome/browser/chromeos/policy/user_network_configuration_updater.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/policy_service.h"
#include "chrome/common/pref_names.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/onc/onc_certificate_importer_impl.h"
#else
#include "chrome/browser/policy/cloud/user_cloud_policy_manager.h"
#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"
#endif

namespace policy {

ProfilePolicyConnector::ProfilePolicyConnector(Profile* profile)
    :
#if defined(OS_CHROMEOS)
      is_primary_user_(false),
      weak_ptr_factory_(this),
#endif
      profile_(profile) {}

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

  bool allow_trusted_certs_from_policy = false;
  chromeos::User* user = NULL;
  if (chromeos::ProfileHelper::IsSigninProfile(profile_)) {
    special_user_policy_provider_.reset(new LoginProfilePolicyProvider(
        connector->GetPolicyService()));
    special_user_policy_provider_->Init();
  } else {
    // |user| should never be NULL except for the signin profile.
    // TODO(joaodasilva): get the |user| that corresponds to the |profile_|
    // from the ProfileHelper, once that's ready.
    chromeos::UserManager* user_manager = chromeos::UserManager::Get();
    user = user_manager->GetActiveUser();
    CHECK(user);
    std::string username = user->email();
    is_primary_user_ =
        chromeos::UserManager::Get()->GetLoggedInUsers().size() == 1;
    if (user->GetType() == chromeos::User::USER_TYPE_PUBLIC_ACCOUNT)
      InitializeDeviceLocalAccountPolicyProvider(username);
    // Allow trusted certs from policy only for managed regular accounts.
    const bool is_managed =
        connector->GetUserAffiliation(username) == USER_AFFILIATION_MANAGED;
    if (is_managed && user->GetType() == chromeos::User::USER_TYPE_REGULAR)
      allow_trusted_certs_from_policy = true;
  }
  if (special_user_policy_provider_)
    providers.push_back(special_user_policy_provider_.get());

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
    if (cloud_policy_manager)
      connector->SetUserPolicyDelegate(cloud_policy_manager);
    else if (special_user_policy_provider_)
      connector->SetUserPolicyDelegate(special_user_policy_provider_.get());

    // A reference to |user| is stored by the NetworkConfigurationUpdater until
    // the Updater is destructed during Shutdown.
    network_configuration_updater_ =
        UserNetworkConfigurationUpdater::CreateForUserPolicy(
            allow_trusted_certs_from_policy,
            *user,
            scoped_ptr<chromeos::onc::CertificateImporter>(
                new chromeos::onc::CertificateImporterImpl),
            policy_service(),
            chromeos::NetworkHandler::Get()
                ->managed_network_configuration_handler());
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
  network_configuration_updater_.reset();
  if (special_user_policy_provider_)
    special_user_policy_provider_->Shutdown();
#endif

#if defined(ENABLE_MANAGED_USERS)
  if (managed_mode_policy_provider_)
    managed_mode_policy_provider_->Shutdown();
#endif
}

#if defined(OS_CHROMEOS)
void ProfilePolicyConnector::SetPolicyCertVerifier(
    PolicyCertVerifier* cert_verifier) {
  if (network_configuration_updater_)
    network_configuration_updater_->SetPolicyCertVerifier(cert_verifier);
}

base::Closure ProfilePolicyConnector::GetPolicyCertTrustedCallback() {
  return base::Bind(&ProfilePolicyConnector::SetUsedPolicyCertificatesOnce,
                    weak_ptr_factory_.GetWeakPtr());
}

void ProfilePolicyConnector::GetWebTrustedCertificates(
    net::CertificateList* certs) const {
  certs->clear();
  if (network_configuration_updater_)
    network_configuration_updater_->GetWebTrustedCertificates(certs);
}
#endif

bool ProfilePolicyConnector::UsedPolicyCertificates() {
#if defined(OS_CHROMEOS)
  return profile_->GetPrefs()->GetBoolean(prefs::kUsedPolicyCertificatesOnce);
#else
  return false;
#endif
}

#if defined(OS_CHROMEOS)
void ProfilePolicyConnector::SetUsedPolicyCertificatesOnce() {
  profile_->GetPrefs()->SetBoolean(prefs::kUsedPolicyCertificatesOnce, true);
}

void ProfilePolicyConnector::InitializeDeviceLocalAccountPolicyProvider(
    const std::string& username) {
  BrowserPolicyConnector* connector =
      g_browser_process->browser_policy_connector();
  DeviceLocalAccountPolicyService* device_local_account_policy_service =
      connector->GetDeviceLocalAccountPolicyService();
  if (!device_local_account_policy_service)
    return;
  special_user_policy_provider_.reset(new DeviceLocalAccountPolicyProvider(
      username, device_local_account_policy_service));
  special_user_policy_provider_->Init();
}
#endif

}  // namespace policy
