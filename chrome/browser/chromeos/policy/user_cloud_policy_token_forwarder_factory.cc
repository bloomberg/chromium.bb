// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/user_cloud_policy_token_forwarder_factory.h"

#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_factory_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_token_forwarder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"

namespace policy {

// static
UserCloudPolicyTokenForwarderFactory*
    UserCloudPolicyTokenForwarderFactory::GetInstance() {
  return Singleton<UserCloudPolicyTokenForwarderFactory>::get();
}

UserCloudPolicyTokenForwarderFactory::UserCloudPolicyTokenForwarderFactory()
    : ProfileKeyedServiceFactory("UserCloudPolicyTokenForwarder",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(TokenServiceFactory::GetInstance());
  DependsOn(UserCloudPolicyManagerFactoryChromeOS::GetInstance());
}

UserCloudPolicyTokenForwarderFactory::~UserCloudPolicyTokenForwarderFactory() {}

ProfileKeyedService*
    UserCloudPolicyTokenForwarderFactory::BuildServiceInstanceFor(
        content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  UserCloudPolicyManagerChromeOS* manager =
      UserCloudPolicyManagerFactoryChromeOS::GetForProfile(profile);
  TokenService* token_service =
      TokenServiceFactory::GetForProfile(profile);
  if (!token_service || !manager)
    return NULL;
  if (manager->IsClientRegistered()) {
    // The CloudPolicyClient is already registered, so the manager doesn't need
    // the refresh token. The manager may have fetched a refresh token if it
    // performed a blocking policy fetch; send it to the TokenService in that
    // case, so that it can be reused for other services.
    if (!manager->oauth2_tokens().refresh_token.empty())
      token_service->UpdateCredentialsWithOAuth2(manager->oauth2_tokens());
    return NULL;
  }
  return new UserCloudPolicyTokenForwarder(manager, token_service);
}

bool UserCloudPolicyTokenForwarderFactory::ServiceIsCreatedWithProfile() const {
  // Create this object when the profile is created so it fetches the token
  // during startup.
  return true;
}

bool UserCloudPolicyTokenForwarderFactory::ServiceIsNULLWhileTesting() const {
  // This is only needed in a handful of tests that manually create the object.
  return true;
}

}  // namespace policy
