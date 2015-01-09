// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_client_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

ChromeSigninClientFactory::ChromeSigninClientFactory()
    : BrowserContextKeyedServiceFactory(
          "ChromeSigninClient",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(SigninErrorControllerFactory::GetInstance());
}

ChromeSigninClientFactory::~ChromeSigninClientFactory() {}

// static
SigninClient* ChromeSigninClientFactory::GetForProfile(Profile* profile) {
  return static_cast<SigninClient*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ChromeSigninClientFactory* ChromeSigninClientFactory::GetInstance() {
  return Singleton<ChromeSigninClientFactory>::get();
}

KeyedService* ChromeSigninClientFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  ChromeSigninClient* client = new ChromeSigninClient(
      profile, SigninErrorControllerFactory::GetForProfile(profile));
  return client;
}
