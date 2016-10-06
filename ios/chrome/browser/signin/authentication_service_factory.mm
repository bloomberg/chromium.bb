// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/authentication_service_factory.h"

#include <memory>
#include <utility>

#include "base/memory/singleton.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/account_tracker_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/oauth2_token_service_factory.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"

// static
AuthenticationService* AuthenticationServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<AuthenticationService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

AuthenticationService* AuthenticationServiceFactory::GetForBrowserStateIfExists(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<AuthenticationService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, false));
}

// static
AuthenticationServiceFactory* AuthenticationServiceFactory::GetInstance() {
  return base::Singleton<
      AuthenticationServiceFactory,
      base::LeakySingletonTraits<AuthenticationServiceFactory>>::get();
}

AuthenticationServiceFactory::AuthenticationServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "AuthenticationService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::AccountTrackerServiceFactory::GetInstance());
  DependsOn(OAuth2TokenServiceFactory::GetInstance());
  DependsOn(ios::SigninManagerFactory::GetInstance());
  DependsOn(SyncSetupServiceFactory::GetInstance());
}

AuthenticationServiceFactory::~AuthenticationServiceFactory() {}

std::unique_ptr<KeyedService>
AuthenticationServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  std::unique_ptr<AuthenticationService> service(new AuthenticationService(
      browser_state,
      OAuth2TokenServiceFactory::GetForBrowserState(browser_state),
      SyncSetupServiceFactory::GetForBrowserState(browser_state)));
  service->Initialize();
  return std::move(service);
}

void AuthenticationServiceFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  AuthenticationService::RegisterPrefs(registry);
}
