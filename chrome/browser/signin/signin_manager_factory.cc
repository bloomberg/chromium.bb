// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager_factory.h"

#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/common/pref_names.h"

SigninManagerFactory::SigninManagerFactory()
    : ProfileKeyedServiceFactory("SigninManager",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(TokenServiceFactory::GetInstance());
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

void SigninManagerFactory::RegisterUserPrefs(PrefServiceSyncable* user_prefs) {
  user_prefs->RegisterStringPref(prefs::kGoogleServicesLastUsername, "",
                                 PrefServiceSyncable::UNSYNCABLE_PREF);
  user_prefs->RegisterStringPref(prefs::kGoogleServicesUsername, "",
                                 PrefServiceSyncable::UNSYNCABLE_PREF);
  user_prefs->RegisterBooleanPref(prefs::kAutologinEnabled, true,
                                  PrefServiceSyncable::UNSYNCABLE_PREF);
  user_prefs->RegisterBooleanPref(prefs::kReverseAutologinEnabled, true,
                                  PrefServiceSyncable::UNSYNCABLE_PREF);
  user_prefs->RegisterListPref(prefs::kReverseAutologinRejectedEmailList,
                               new ListValue,
                               PrefServiceSyncable::UNSYNCABLE_PREF);
  user_prefs->RegisterBooleanPref(prefs::kIsGooglePlusUser, false,
                                 PrefServiceSyncable::UNSYNCABLE_PREF);
}

// static
void SigninManagerFactory::RegisterPrefs(PrefServiceSimple* local_state) {
  local_state->RegisterStringPref(prefs::kGoogleServicesUsernamePattern, "");
}

ProfileKeyedService* SigninManagerFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  SigninManager* service = new SigninManager();
  service->Initialize(profile);
  return service;
}
