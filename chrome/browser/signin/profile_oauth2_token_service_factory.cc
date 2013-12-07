// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

#if defined(OS_ANDROID)
#include "chrome/browser/signin/android_profile_oauth2_token_service.h"
#else
#include "chrome/browser/signin/mutable_profile_oauth2_token_service.h"
#endif

ProfileOAuth2TokenServiceFactory::ProfileOAuth2TokenServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "ProfileOAuth2TokenService",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(GlobalErrorServiceFactory::GetInstance());
  DependsOn(WebDataServiceFactory::GetInstance());
}

ProfileOAuth2TokenServiceFactory::~ProfileOAuth2TokenServiceFactory() {
}

ProfileOAuth2TokenService*
ProfileOAuth2TokenServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<ProfileOAuth2TokenService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ProfileOAuth2TokenServiceFactory::PlatformSpecificOAuth2TokenService*
ProfileOAuth2TokenServiceFactory::GetPlatformSpecificForProfile(
    Profile* profile) {
  return static_cast<PlatformSpecificOAuth2TokenService*>(
      GetForProfile(profile));
}

// static
ProfileOAuth2TokenServiceFactory*
    ProfileOAuth2TokenServiceFactory::GetInstance() {
  return Singleton<ProfileOAuth2TokenServiceFactory>::get();
}

BrowserContextKeyedService*
ProfileOAuth2TokenServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  PlatformSpecificOAuth2TokenService* service =
      new PlatformSpecificOAuth2TokenService();
  service->Initialize(profile);
  return service;
}
