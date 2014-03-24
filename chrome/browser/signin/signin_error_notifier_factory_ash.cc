// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_error_notifier_factory_ash.h"

#include "ash/shell.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_error_notifier_ash.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"

SigninErrorNotifierFactory::SigninErrorNotifierFactory()
    : BrowserContextKeyedServiceFactory(
        "SigninErrorNotifier",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
}

SigninErrorNotifierFactory::~SigninErrorNotifierFactory() {}

// static
SigninErrorNotifier* SigninErrorNotifierFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SigninErrorNotifier*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SigninErrorNotifierFactory* SigninErrorNotifierFactory::GetInstance() {
  return Singleton<SigninErrorNotifierFactory>::get();
}

KeyedService* SigninErrorNotifierFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!ash::Shell::HasInstance())
    return NULL;

  Profile* profile = static_cast<Profile*>(context);

  SigninErrorController* controller =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile)->
      signin_error_controller();

  return new SigninErrorNotifier(controller, profile);
}
