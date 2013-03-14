// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/token_cache/token_cache_service_factory.h"

#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/token_cache/token_cache_service.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

// static
extensions::TokenCacheService*
TokenCacheServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<extensions::TokenCacheService*>(
      GetInstance()->GetServiceForProfile(profile, true));
 }

// static
TokenCacheServiceFactory* TokenCacheServiceFactory::GetInstance() {
  return Singleton<TokenCacheServiceFactory>::get();
}

TokenCacheServiceFactory::TokenCacheServiceFactory()
    : ProfileKeyedServiceFactory("TokenCacheService",
                                 ProfileDependencyManager::GetInstance()) {
}

TokenCacheServiceFactory::~TokenCacheServiceFactory() {
}

ProfileKeyedService* TokenCacheServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new extensions::TokenCacheService(profile);
}
