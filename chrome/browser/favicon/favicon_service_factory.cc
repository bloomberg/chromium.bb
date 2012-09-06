// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/favicon_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/common/pref_names.h"

// static
FaviconService* FaviconServiceFactory::GetForProfile(
    Profile* profile, Profile::ServiceAccessType sat) {
  if (!profile->IsOffTheRecord()) {
    return static_cast<FaviconService*>(
        GetInstance()->GetServiceForProfile(profile, true));
  } else if (sat == Profile::EXPLICIT_ACCESS) {
    // Profile must be OffTheRecord in this case.
    return static_cast<FaviconService*>(
        GetInstance()->GetServiceForProfile(
            profile->GetOriginalProfile(), true));
  }

  // Profile is OffTheRecord without access.
  NOTREACHED() << "This profile is OffTheRecord";
  return NULL;
}

// static
FaviconServiceFactory* FaviconServiceFactory::GetInstance() {
  return Singleton<FaviconServiceFactory>::get();
}

FaviconServiceFactory::FaviconServiceFactory()
    : ProfileKeyedServiceFactory("FaviconService",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(HistoryServiceFactory::GetInstance());
}

FaviconServiceFactory::~FaviconServiceFactory() {}

ProfileKeyedService* FaviconServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  HistoryService* history_service = HistoryServiceFactory::GetForProfile(
      profile, Profile::EXPLICIT_ACCESS);
  return new FaviconService(history_service);
}

bool FaviconServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
