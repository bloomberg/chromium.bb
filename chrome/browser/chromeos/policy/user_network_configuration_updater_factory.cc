// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/user_network_configuration_updater_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/policy/user_network_configuration_updater.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/network/network_handler.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_type.h"

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

KeyedService* UserNetworkConfigurationUpdaterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  if (chromeos::ProfileHelper::IsSigninProfile(profile))
    return NULL;  // On the login screen only device network policies apply.

  user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  DCHECK(user);
  // Currently, only the network policy of the primary user is supported. See
  // also http://crbug.com/310685 .
  if (user != chromeos::UserManager::Get()->GetPrimaryUser())
    return NULL;

  const bool allow_trusted_certs_from_policy =
      user->GetType() == user_manager::USER_TYPE_REGULAR;

  ProfilePolicyConnector* profile_connector =
      ProfilePolicyConnectorFactory::GetForProfile(profile);

  return UserNetworkConfigurationUpdater::CreateForUserPolicy(
      profile,
      allow_trusted_certs_from_policy,
      *user,
      profile_connector->policy_service(),
      chromeos::NetworkHandler::Get()->managed_network_configuration_handler())
      .release();
}

}  // namespace policy
