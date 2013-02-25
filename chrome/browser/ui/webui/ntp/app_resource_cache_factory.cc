// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/app_resource_cache_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache.h"

// static
NTPResourceCache* AppResourceCacheFactory::GetForProfile(Profile* profile) {
  return static_cast<NTPResourceCache*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
AppResourceCacheFactory* AppResourceCacheFactory::GetInstance() {
  return Singleton<AppResourceCacheFactory>::get();
}

AppResourceCacheFactory::AppResourceCacheFactory()
    : ProfileKeyedServiceFactory("AppResourceCache",
                                 ProfileDependencyManager::GetInstance()) {
#if defined(ENABLE_THEMES)
  DependsOn(ThemeServiceFactory::GetInstance());
#endif
}

AppResourceCacheFactory::~AppResourceCacheFactory() {}

ProfileKeyedService* AppResourceCacheFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new NTPResourceCache(profile);
}

bool AppResourceCacheFactory::ServiceRedirectedInIncognito() const {
  return true;
}
