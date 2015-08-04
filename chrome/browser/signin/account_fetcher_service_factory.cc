// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_fetcher_service_factory.h"

#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/core/browser/account_fetcher_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"

AccountFetcherServiceFactory::AccountFetcherServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "AccountFetcherServiceFactory",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(AccountTrackerServiceFactory::GetInstance());
  DependsOn(ChromeSigninClientFactory::GetInstance());
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
  DependsOn(invalidation::ProfileInvalidationProviderFactory::GetInstance());
}

AccountFetcherServiceFactory::~AccountFetcherServiceFactory() {}

// static
AccountFetcherService* AccountFetcherServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AccountFetcherService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AccountFetcherServiceFactory* AccountFetcherServiceFactory::GetInstance() {
  return Singleton<AccountFetcherServiceFactory>::get();
}

void AccountFetcherServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  AccountFetcherService::RegisterPrefs(registry);
}

KeyedService* AccountFetcherServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  AccountFetcherService* service = new AccountFetcherService();
  invalidation::ProfileInvalidationProvider* invalidation_provider =
      invalidation::ProfileInvalidationProviderFactory::GetForProfile(profile);
  // Chrome OS login and guest profiles do not support invalidation. This is
  // fine as they do not have GAIA credentials anyway. http://crbug.com/358169
  invalidation::InvalidationService* invalidation_service =
      invalidation_provider ? invalidation_provider->GetInvalidationService()
                            : nullptr;
  service->Initialize(ChromeSigninClientFactory::GetForProfile(profile),
                      ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
                      AccountTrackerServiceFactory::GetForProfile(profile),
                      invalidation_service);
  return service;
}
