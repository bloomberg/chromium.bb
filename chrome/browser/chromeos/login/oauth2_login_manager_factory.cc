// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/oauth2_login_manager_factory.h"

#include "chrome/browser/chromeos/login/oauth2_login_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

namespace chromeos {

OAuth2LoginManagerFactory::OAuth2LoginManagerFactory()
    : BrowserContextKeyedServiceFactory(
        "OAuth2LoginManager",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(GlobalErrorServiceFactory::GetInstance());
  DependsOn(SigninManagerFactory::GetInstance());
  DependsOn(TokenServiceFactory::GetInstance());
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
}

OAuth2LoginManagerFactory::~OAuth2LoginManagerFactory() {
}

// static
OAuth2LoginManager* OAuth2LoginManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<OAuth2LoginManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
OAuth2LoginManagerFactory*
    OAuth2LoginManagerFactory::GetInstance() {
  return Singleton<OAuth2LoginManagerFactory>::get();
}

BrowserContextKeyedService*
OAuth2LoginManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  OAuth2LoginManager* service;
  service = new OAuth2LoginManager(profile);
  return service;
}

}  // namespace chromeos
