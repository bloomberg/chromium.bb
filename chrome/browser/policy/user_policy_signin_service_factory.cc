// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/user_policy_signin_service_factory.h"

#include "chrome/browser/policy/user_policy_signin_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/common/pref_names.h"

namespace policy {

UserPolicySigninServiceFactory::UserPolicySigninServiceFactory()
    : ProfileKeyedServiceFactory("UserPolicySigninService",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(TokenServiceFactory::GetInstance());
  DependsOn(SigninManagerFactory::GetInstance());
}

UserPolicySigninServiceFactory::~UserPolicySigninServiceFactory() {}

// static
UserPolicySigninService* UserPolicySigninServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<UserPolicySigninService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
UserPolicySigninServiceFactory* UserPolicySigninServiceFactory::GetInstance() {
  return Singleton<UserPolicySigninServiceFactory>::get();
}

ProfileKeyedService* UserPolicySigninServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new UserPolicySigninService(profile,
                                     profile->GetUserCloudPolicyManager());
}

bool UserPolicySigninServiceFactory::ServiceIsCreatedWithProfile() const {
  // Create this object when the profile is created so it can track any
  // user signin activity.
  return true;
}

void UserPolicySigninServiceFactory::RegisterUserPrefs(
    PrefService* user_prefs) {
  user_prefs->RegisterBooleanPref(prefs::kLoadCloudPolicyOnSignin,
                                  false, PrefService::UNSYNCABLE_PREF);
}

}  // namespace policy
