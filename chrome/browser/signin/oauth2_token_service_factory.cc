// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/oauth2_token_service_factory.h"

#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/signin/oauth2_token_service.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service_factory.h"

OAuth2TokenServiceFactory::OAuth2TokenServiceFactory()
    : ProfileKeyedServiceFactory("OAuth2TokenService",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(SigninManagerFactory::GetInstance());
  DependsOn(TokenServiceFactory::GetInstance());
}

OAuth2TokenServiceFactory::~OAuth2TokenServiceFactory() {
}

// static
OAuth2TokenService* OAuth2TokenServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<OAuth2TokenService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
OAuth2TokenServiceFactory* OAuth2TokenServiceFactory::GetInstance() {
  return Singleton<OAuth2TokenServiceFactory>::get();
}

ProfileKeyedService* OAuth2TokenServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  OAuth2TokenService* service = new OAuth2TokenService();
  service->Initialize(profile);
  return service;
}
