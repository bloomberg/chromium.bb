// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/authentication_service_factory.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/account_tracker_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_delegate.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
AuthenticationService* AuthenticationServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  AuthenticationService* service = static_cast<AuthenticationService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
  CHECK(!service || service->initialized());
  return service;
}

// static
AuthenticationServiceFactory* AuthenticationServiceFactory::GetInstance() {
  static base::NoDestructor<AuthenticationServiceFactory> instance;
  return instance.get();
}

// static
void AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
    ios::ChromeBrowserState* browser_state,
    std::unique_ptr<AuthenticationServiceDelegate> delegate) {
  AuthenticationService* service = static_cast<AuthenticationService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
  CHECK(service && !service->initialized());
  service->Initialize(std::move(delegate));
}

AuthenticationServiceFactory::AuthenticationServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "AuthenticationService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::AccountTrackerServiceFactory::GetInstance());
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SyncSetupServiceFactory::GetInstance());
  DependsOn(ProfileSyncServiceFactory::GetInstance());
}

AuthenticationServiceFactory::~AuthenticationServiceFactory() {}

std::unique_ptr<KeyedService>
AuthenticationServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<AuthenticationService>(
      browser_state->GetPrefs(),
      ProfileOAuth2TokenServiceFactory::GetForBrowserState(browser_state),
      SyncSetupServiceFactory::GetForBrowserState(browser_state),
      ios::AccountTrackerServiceFactory::GetForBrowserState(browser_state),
      IdentityManagerFactory::GetForBrowserState(browser_state),
      ProfileSyncServiceFactory::GetForBrowserState(browser_state));
}

void AuthenticationServiceFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  AuthenticationService::RegisterPrefs(registry);
}

bool AuthenticationServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
