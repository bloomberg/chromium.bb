// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_error_controller_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

SigninErrorControllerFactory::SigninErrorControllerFactory()
    : BrowserContextKeyedServiceFactory(
          "SigninErrorController",
          BrowserContextDependencyManager::GetInstance()) {}

SigninErrorControllerFactory::~SigninErrorControllerFactory() {}

// static
SigninErrorController* SigninErrorControllerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SigninErrorController*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SigninErrorControllerFactory* SigninErrorControllerFactory::GetInstance() {
  return Singleton<SigninErrorControllerFactory>::get();
}

KeyedService* SigninErrorControllerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new SigninErrorController();
}
