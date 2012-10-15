// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_restore_service_factory.h"

#include "chrome/browser/extensions/app_restore_service.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace extensions {

// static
AppRestoreService* AppRestoreServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<AppRestoreService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
void AppRestoreServiceFactory::ResetForProfile(Profile* profile) {
  AppRestoreServiceFactory* factory = GetInstance();
  factory->ProfileShutdown(profile);
  factory->ProfileDestroyed(profile);
}

AppRestoreServiceFactory* AppRestoreServiceFactory::GetInstance() {
  return Singleton<AppRestoreServiceFactory>::get();
}

AppRestoreServiceFactory::AppRestoreServiceFactory()
    : ProfileKeyedServiceFactory("AppRestoreService",
                                 ProfileDependencyManager::GetInstance()) {
}

AppRestoreServiceFactory::~AppRestoreServiceFactory() {
}

ProfileKeyedService* AppRestoreServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  AppRestoreService* service = NULL;
  service = new AppRestoreService(profile);
  return service;
}

bool AppRestoreServiceFactory::ServiceIsCreatedWithProfile() const {
  return true;
}

}  // namespace extensions
