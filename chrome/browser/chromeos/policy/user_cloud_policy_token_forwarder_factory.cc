// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/user_cloud_policy_token_forwarder_factory.h"

#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_factory_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_token_forwarder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

namespace policy {

// static
UserCloudPolicyTokenForwarderFactory*
    UserCloudPolicyTokenForwarderFactory::GetInstance() {
  return Singleton<UserCloudPolicyTokenForwarderFactory>::get();
}

UserCloudPolicyTokenForwarderFactory::UserCloudPolicyTokenForwarderFactory()
    : BrowserContextKeyedServiceFactory(
        "UserCloudPolicyTokenForwarder",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
  DependsOn(UserCloudPolicyManagerFactoryChromeOS::GetInstance());
}

UserCloudPolicyTokenForwarderFactory::~UserCloudPolicyTokenForwarderFactory() {}

BrowserContextKeyedService*
    UserCloudPolicyTokenForwarderFactory::BuildServiceInstanceFor(
        content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  UserCloudPolicyManagerChromeOS* manager =
      UserCloudPolicyManagerFactoryChromeOS::GetForProfile(profile);
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  if (!token_service || !manager)
    return NULL;
  return new UserCloudPolicyTokenForwarder(manager, token_service);
}

bool UserCloudPolicyTokenForwarderFactory::
ServiceIsCreatedWithBrowserContext() const {
  // Create this object when the profile is created so it fetches the token
  // during startup.
  return true;
}

bool UserCloudPolicyTokenForwarderFactory::ServiceIsNULLWhileTesting() const {
  // This is only needed in a handful of tests that manually create the object.
  return true;
}

}  // namespace policy
