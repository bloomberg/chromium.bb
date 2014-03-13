// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/token_cache/token_cache_service_factory.h"

#include "chrome/browser/extensions/token_cache/token_cache_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
extensions::TokenCacheService*
TokenCacheServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<extensions::TokenCacheService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
 }

// static
TokenCacheServiceFactory* TokenCacheServiceFactory::GetInstance() {
  return Singleton<TokenCacheServiceFactory>::get();
}

TokenCacheServiceFactory::TokenCacheServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "TokenCacheService",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(SigninManagerFactory::GetInstance());
}

TokenCacheServiceFactory::~TokenCacheServiceFactory() {
}

KeyedService* TokenCacheServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new extensions::TokenCacheService(static_cast<Profile*>(profile));
}
