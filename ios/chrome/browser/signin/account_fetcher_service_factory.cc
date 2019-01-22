// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/account_fetcher_service_factory.h"

#include <utility>

#include "base/no_destructor.h"
#include "components/image_fetcher/ios/ios_image_decoder_impl.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/account_fetcher_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/account_tracker_service_factory.h"
#include "ios/chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "ios/chrome/browser/signin/signin_client_factory.h"

namespace ios {

AccountFetcherServiceFactory::AccountFetcherServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "AccountFetcherService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(AccountTrackerServiceFactory::GetInstance());
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
  DependsOn(SigninClientFactory::GetInstance());
}

AccountFetcherServiceFactory::~AccountFetcherServiceFactory() {}

// static
AccountFetcherService* AccountFetcherServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<AccountFetcherService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
AccountFetcherServiceFactory* AccountFetcherServiceFactory::GetInstance() {
  static base::NoDestructor<AccountFetcherServiceFactory> instance;
  return instance.get();
}

void AccountFetcherServiceFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  AccountFetcherService::RegisterPrefs(registry);
}

std::unique_ptr<KeyedService>
AccountFetcherServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  std::unique_ptr<AccountFetcherService> service(new AccountFetcherService());
  service->Initialize(
      SigninClientFactory::GetForBrowserState(browser_state),
      ProfileOAuth2TokenServiceFactory::GetForBrowserState(browser_state),
      ios::AccountTrackerServiceFactory::GetForBrowserState(browser_state),
      image_fetcher::CreateIOSImageDecoder());
  return service;
}

}  // namespace ios
