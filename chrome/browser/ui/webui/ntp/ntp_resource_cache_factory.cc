// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/ntp_resource_cache_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache.h"

// static
NTPResourceCache* NTPResourceCacheFactory::GetForProfile(Profile* profile) {
  return static_cast<NTPResourceCache*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
NTPResourceCacheFactory* NTPResourceCacheFactory::GetInstance() {
  return Singleton<NTPResourceCacheFactory>::get();
}

NTPResourceCacheFactory::NTPResourceCacheFactory()
    : ProfileKeyedServiceFactory("NTPResourceCache",
                                 ProfileDependencyManager::GetInstance()) {
#if defined(ENABLE_THEMES)
  DependsOn(ThemeServiceFactory::GetInstance());
#endif
}

NTPResourceCacheFactory::~NTPResourceCacheFactory() {}

ProfileKeyedService* NTPResourceCacheFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new NTPResourceCache(profile);
}

bool NTPResourceCacheFactory::ServiceRedirectedInIncognito() const {
  return true;
}
