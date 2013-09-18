// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"

#include "base/memory/ref_counted.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "net/url_request/url_request_context_getter.h"

#if defined(OS_ANDROID)
#include "chrome/browser/policy/cloud/user_policy_signin_service_android.h"
#else
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#endif

namespace policy {

namespace {

// Used only for testing.
DeviceManagementService* g_device_management_service = NULL;

}  // namespace

UserPolicySigninServiceFactory::UserPolicySigninServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "UserPolicySigninService",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
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

// static
void UserPolicySigninServiceFactory::SetDeviceManagementServiceForTesting(
    DeviceManagementService* device_management_service) {
  g_device_management_service = device_management_service;
}

BrowserContextKeyedService*
UserPolicySigninServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  BrowserPolicyConnector* connector =
      g_browser_process->browser_policy_connector();
  DeviceManagementService* device_management_service =
      g_device_management_service ? g_device_management_service
                                  : connector->device_management_service();
  // TODO(atwilson): Inject SigninManager here or remove the dependency
  // entirely. http://crbug.com/276270.
  return new UserPolicySigninService(
      profile,
      g_browser_process->local_state(),
      g_browser_process->system_request_context(),
      device_management_service,
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile));
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
