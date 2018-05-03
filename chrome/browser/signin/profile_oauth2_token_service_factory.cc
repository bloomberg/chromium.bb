// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/web_data_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"

#if defined(OS_ANDROID)
#include "chrome/browser/signin/oauth2_token_service_delegate_android.h"
#else
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/signin/mutable_profile_oauth2_token_service_delegate.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/signin/core/browser/cookie_settings_util.h"
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

void ProfileOAuth2TokenServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  ProfileOAuth2TokenService::RegisterProfilePrefs(registry);
#if !defined(OS_ANDROID)
  MutableProfileOAuth2TokenServiceDelegate::RegisterProfilePrefs(registry);
#endif
}

KeyedService* ProfileOAuth2TokenServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
#if defined(OS_ANDROID)
  auto delegate = std::make_unique<OAuth2TokenServiceDelegateAndroid>(
      AccountTrackerServiceFactory::GetInstance()->GetForProfile(profile));
#else
  signin::AccountConsistencyMethod account_consistency =
      AccountConsistencyModeManager::GetMethodForProfile(profile);
  // When signin cookies are cleared on exit and Dice is enabled, all tokens
  // should also be cleared.
  bool revoke_all_tokens_on_load =
      (account_consistency == signin::AccountConsistencyMethod::kDice) &&
      signin::SettingsDeleteSigninCookiesOnExit(
          CookieSettingsFactory::GetForProfile(profile).get());

  auto delegate = std::make_unique<MutableProfileOAuth2TokenServiceDelegate>(
      ChromeSigninClientFactory::GetInstance()->GetForProfile(profile),
      SigninErrorControllerFactory::GetInstance()->GetForProfile(profile),
      AccountTrackerServiceFactory::GetInstance()->GetForProfile(profile),
      account_consistency, revoke_all_tokens_on_load);
#endif
  ProfileOAuth2TokenService* service =
      new ProfileOAuth2TokenService(std::move(delegate));
  return service;
}
