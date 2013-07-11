// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "components/user_prefs/pref_registry_syncable.h"

#if defined(OS_ANDROID)
#include "chrome/browser/policy/cloud/user_policy_signin_service_android.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#else
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#endif

namespace policy {

UserPolicySigninServiceFactory::UserPolicySigninServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "UserPolicySigninService",
        BrowserContextDependencyManager::GetInstance()) {
#if defined(OS_ANDROID)
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
#else
  DependsOn(TokenServiceFactory::GetInstance());
#endif
  DependsOn(SigninManagerFactory::GetInstance());
  DependsOn(UserCloudPolicyManagerFactory::GetInstance());
}

UserPolicySigninServiceFactory::~UserPolicySigninServiceFactory() {}

// static
UserPolicySigninService* UserPolicySigninServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<UserPolicySigninService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
UserPolicySigninServiceFactory* UserPolicySigninServiceFactory::GetInstance() {
  return Singleton<UserPolicySigninServiceFactory>::get();
}

BrowserContextKeyedService*
UserPolicySigninServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new UserPolicySigninService(static_cast<Profile*>(profile));
}

bool
UserPolicySigninServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  // Create this object when the profile is created so it can track any
  // user signin activity.
  return true;
}

void UserPolicySigninServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  user_prefs->RegisterBooleanPref(
      prefs::kDisableCloudPolicyOnSignin,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
#if defined(OS_ANDROID)
  user_prefs->RegisterInt64Pref(
      prefs::kLastPolicyCheckTime,
      0,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
#endif
}

}  // namespace policy
