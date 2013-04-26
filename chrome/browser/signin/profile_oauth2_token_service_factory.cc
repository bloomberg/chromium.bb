// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service_factory.h"

#if defined(OS_ANDROID)
#include "chrome/browser/signin/android_profile_oauth2_token_service.h"
#endif

ProfileOAuth2TokenServiceFactory::ProfileOAuth2TokenServiceFactory()
    : ProfileKeyedServiceFactory("ProfileOAuth2TokenService",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(SigninManagerFactory::GetInstance());
  DependsOn(TokenServiceFactory::GetInstance());
}

ProfileOAuth2TokenServiceFactory::~ProfileOAuth2TokenServiceFactory() {
}

// static
ProfileOAuth2TokenService* ProfileOAuth2TokenServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ProfileOAuth2TokenService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
ProfileOAuth2TokenServiceFactory*
    ProfileOAuth2TokenServiceFactory::GetInstance() {
  return Singleton<ProfileOAuth2TokenServiceFactory>::get();
}

ProfileKeyedService* ProfileOAuth2TokenServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  ProfileOAuth2TokenService* service;
#if defined(OS_ANDROID)
  service = new AndroidProfileOAuth2TokenService(profile->GetRequestContext());
#else
  service = new ProfileOAuth2TokenService(profile->GetRequestContext());
#endif
  service->Initialize(profile);
  return service;
}
