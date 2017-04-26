// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/one_google_bar/one_google_bar_service_factory.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/google/google_url_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_fetcher_impl.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/browser_context.h"

// static
OneGoogleBarService* OneGoogleBarServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<OneGoogleBarService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
OneGoogleBarServiceFactory* OneGoogleBarServiceFactory::GetInstance() {
  return base::Singleton<OneGoogleBarServiceFactory>::get();
}

OneGoogleBarServiceFactory::OneGoogleBarServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "OneGoogleBarService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(SigninManagerFactory::GetInstance());
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
  DependsOn(GoogleURLTrackerFactory::GetInstance());
}

OneGoogleBarServiceFactory::~OneGoogleBarServiceFactory() = default;

KeyedService* OneGoogleBarServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  OAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  GoogleURLTracker* google_url_tracker =
      GoogleURLTrackerFactory::GetForProfile(profile);
  return new OneGoogleBarService(
      signin_manager, base::MakeUnique<OneGoogleBarFetcherImpl>(
                          signin_manager, token_service,
                          profile->GetRequestContext(), google_url_tracker));
}
