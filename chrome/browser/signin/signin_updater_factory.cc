// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_updater_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_updater.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

SigninUpdaterFactory::SigninUpdaterFactory()
    : BrowserContextKeyedServiceFactory(
          "SigninUpdater",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(SigninManagerFactory::GetInstance());
}

SigninUpdaterFactory::~SigninUpdaterFactory() {}

// static
SigninUpdaterFactory* SigninUpdaterFactory::GetInstance() {
  return base::Singleton<SigninUpdaterFactory>::get();
}

// static
SigninUpdater* SigninUpdaterFactory::GetForProfile(Profile* profile) {
  return static_cast<SigninUpdater*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

KeyedService* SigninUpdaterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  return new SigninUpdater(signin_manager);
}

bool SigninUpdaterFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}
