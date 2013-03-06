// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager_factory.h"

#include "base/prefs/pref_registry_simple.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"

SigninManagerFactory::SigninManagerFactory()
    : ProfileKeyedServiceFactory("SigninManager",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(TokenServiceFactory::GetInstance());
  DependsOn(GlobalErrorServiceFactory::GetInstance());
}

SigninManagerFactory::~SigninManagerFactory() {}

// static
SigninManager* SigninManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<SigninManager*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
SigninManager* SigninManagerFactory::GetForProfileIfExists(Profile* profile) {
  return static_cast<SigninManager*>(
      GetInstance()->GetServiceForProfile(profile, false));
}

// static
SigninManagerFactory* SigninManagerFactory::GetInstance() {
  return Singleton<SigninManagerFactory>::get();
}

void SigninManagerFactory::RegisterUserPrefs(PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(prefs::kGoogleServicesLastUsername, "",
                               PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kGoogleServicesUsername, "",
                               PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kAutologinEnabled, true,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kReverseAutologinEnabled, true,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(prefs::kReverseAutologinRejectedEmailList,
                             new ListValue,
                             PrefRegistrySyncable::UNSYNCABLE_PREF);
}

// static
void SigninManagerFactory::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kGoogleServicesUsernamePattern, "");
}

ProfileKeyedService* SigninManagerFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  SigninManager* service = new SigninManager();
  service->Initialize(profile);
  return service;
}
