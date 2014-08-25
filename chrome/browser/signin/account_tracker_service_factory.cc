// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_tracker_service_factory.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/common/signin_pref_names.h"

AccountTrackerServiceFactory::AccountTrackerServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "AccountTrackerServiceFactory",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
}

AccountTrackerServiceFactory::~AccountTrackerServiceFactory() {
}

// static
AccountTrackerService*
AccountTrackerServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<AccountTrackerService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AccountTrackerServiceFactory* AccountTrackerServiceFactory::GetInstance() {
  return Singleton<AccountTrackerServiceFactory>::get();
}

void AccountTrackerServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(
      AccountTrackerService::kAccountInfoPref,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kAccountIdMigrationState,
      AccountTrackerService::MIGRATION_NOT_STARTED,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

KeyedService* AccountTrackerServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  AccountTrackerService* service = new AccountTrackerService();
  service->Initialize(
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
      profile->GetPrefs(),
      profile->GetRequestContext());
  return service;
}
