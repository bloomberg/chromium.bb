// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_user_service_factory.h"

#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

// static
ManagedUserService* ManagedUserServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<ManagedUserService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ManagedUserServiceFactory* ManagedUserServiceFactory::GetInstance() {
  return Singleton<ManagedUserServiceFactory>::get();
}

// static
BrowserContextKeyedService* ManagedUserServiceFactory::BuildInstanceFor(
    Profile* profile) {
  return new ManagedUserService(profile);
}

ManagedUserServiceFactory::ManagedUserServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "ManagedUserService",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
}

ManagedUserServiceFactory::~ManagedUserServiceFactory() {}

content::BrowserContext* ManagedUserServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

BrowserContextKeyedService* ManagedUserServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return BuildInstanceFor(static_cast<Profile*>(profile));
}
