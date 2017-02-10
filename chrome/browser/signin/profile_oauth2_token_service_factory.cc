// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/web_data_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"

#if defined(OS_ANDROID)
#include "chrome/browser/signin/oauth2_token_service_delegate_android.h"
#else
#include "chrome/browser/signin/mutable_profile_oauth2_token_service_delegate.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#endif

ProfileOAuth2TokenServiceFactory::ProfileOAuth2TokenServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "ProfileOAuth2TokenService",
        BrowserContextDependencyManager::GetInstance()) {
#if !defined(OS_ANDROID)
  DependsOn(GlobalErrorServiceFactory::GetInstance());
#endif
  DependsOn(WebDataServiceFactory::GetInstance());
  DependsOn(ChromeSigninClientFactory::GetInstance());
  DependsOn(SigninErrorControllerFactory::GetInstance());
  DependsOn(AccountTrackerServiceFactory::GetInstance());
}

ProfileOAuth2TokenServiceFactory::~ProfileOAuth2TokenServiceFactory() {
}

ProfileOAuth2TokenService*
ProfileOAuth2TokenServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<ProfileOAuth2TokenService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ProfileOAuth2TokenServiceFactory*
    ProfileOAuth2TokenServiceFactory::GetInstance() {
  return base::Singleton<ProfileOAuth2TokenServiceFactory>::get();
}

KeyedService* ProfileOAuth2TokenServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
#if defined(OS_ANDROID)
  auto delegate = base::MakeUnique<OAuth2TokenServiceDelegateAndroid>(
      AccountTrackerServiceFactory::GetInstance()->GetForProfile(profile));
#else
  auto delegate = base::MakeUnique<MutableProfileOAuth2TokenServiceDelegate>(
      ChromeSigninClientFactory::GetInstance()->GetForProfile(profile),
      SigninErrorControllerFactory::GetInstance()->GetForProfile(profile),
      AccountTrackerServiceFactory::GetInstance()->GetForProfile(profile));
#endif
  ProfileOAuth2TokenService* service =
      new ProfileOAuth2TokenService(std::move(delegate));
  return service;
}
