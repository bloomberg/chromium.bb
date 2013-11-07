// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/user_network_configuration_updater_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/policy/user_network_configuration_updater.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/cloud/cloud_policy_constants.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/onc/onc_certificate_importer_impl.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

namespace policy {

// static
UserNetworkConfigurationUpdater*
UserNetworkConfigurationUpdaterFactory::GetForProfile(Profile* profile) {
  return static_cast<UserNetworkConfigurationUpdater*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
UserNetworkConfigurationUpdaterFactory*
UserNetworkConfigurationUpdaterFactory::GetInstance() {
  return Singleton<UserNetworkConfigurationUpdaterFactory>::get();
}

UserNetworkConfigurationUpdaterFactory::UserNetworkConfigurationUpdaterFactory()
    : BrowserContextKeyedServiceFactory(
          "UserNetworkConfigurationUpdater",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ProfilePolicyConnectorFactory::GetInstance());
}

UserNetworkConfigurationUpdaterFactory::
    ~UserNetworkConfigurationUpdaterFactory() {}

content::BrowserContext*
UserNetworkConfigurationUpdaterFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool
UserNetworkConfigurationUpdaterFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool UserNetworkConfigurationUpdaterFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

BrowserContextKeyedService*
UserNetworkConfigurationUpdaterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  if (chromeos::ProfileHelper::IsSigninProfile(profile))
    return NULL;  // On the login screen only device network policies apply.

  chromeos::UserManager* user_manager = chromeos::UserManager::Get();
  chromeos::User* user = user_manager->GetUserByProfile(profile);
  DCHECK(user);
  // Currently, only the network policy of the primary user is supported. See
  // also http://crbug.com/310685 .
  if (user != user_manager->GetPrimaryUser())
    return NULL;

  BrowserPolicyConnector* browser_connector =
      g_browser_process->browser_policy_connector();

  // Allow trusted certs from policy only for accounts with managed user
  // affiliation, i.e users that are managed by the same domain as the device.
  bool allow_trusted_certs_from_policy =
      browser_connector->GetUserAffiliation(user->email()) ==
          USER_AFFILIATION_MANAGED &&
      user->GetType() == chromeos::User::USER_TYPE_REGULAR;

  ProfilePolicyConnector* profile_connector =
      ProfilePolicyConnectorFactory::GetForProfile(profile);

  return UserNetworkConfigurationUpdater::CreateForUserPolicy(
      allow_trusted_certs_from_policy,
      *user,
      scoped_ptr<chromeos::onc::CertificateImporter>(
          new chromeos::onc::CertificateImporterImpl),
      profile_connector->policy_service(),
      chromeos::NetworkHandler::Get()->managed_network_configuration_handler())
      .release();
}

}  // namespace policy
