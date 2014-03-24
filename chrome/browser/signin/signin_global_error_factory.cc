// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_global_error_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_global_error.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"

#if defined(USE_ASH)
#include "ash/shell.h"
#endif

SigninGlobalErrorFactory::SigninGlobalErrorFactory()
    : BrowserContextKeyedServiceFactory(
        "SigninGlobalError",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
  DependsOn(GlobalErrorServiceFactory::GetInstance());
}

SigninGlobalErrorFactory::~SigninGlobalErrorFactory() {}

// static
SigninGlobalError* SigninGlobalErrorFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SigninGlobalError*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SigninGlobalErrorFactory* SigninGlobalErrorFactory::GetInstance() {
  return Singleton<SigninGlobalErrorFactory>::get();
}

KeyedService* SigninGlobalErrorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
#if defined(USE_ASH)
  if (ash::Shell::HasInstance())
    return NULL;
#endif

  Profile* profile = static_cast<Profile*>(context);

  SigninErrorController* controller =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile)->
      signin_error_controller();

  return new SigninGlobalError(controller, profile);
}
