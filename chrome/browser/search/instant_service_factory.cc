// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_service_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

// static
InstantService* InstantServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<InstantService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
InstantServiceFactory* InstantServiceFactory::GetInstance() {
  return Singleton<InstantServiceFactory>::get();
}

InstantServiceFactory::InstantServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "InstantService",
        BrowserContextDependencyManager::GetInstance()) {
  // No dependencies.
}

InstantServiceFactory::~InstantServiceFactory() {
}

content::BrowserContext* InstantServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

BrowserContextKeyedService* InstantServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new InstantService(static_cast<Profile*>(profile));
}
