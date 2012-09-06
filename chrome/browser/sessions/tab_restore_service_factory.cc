// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/tab_restore_service_factory.h"

#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/sessions/tab_restore_service.h"

// static
TabRestoreService* TabRestoreServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<TabRestoreService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
void TabRestoreServiceFactory::ResetForProfile(Profile* profile) {
  TabRestoreServiceFactory* factory = GetInstance();
  factory->ProfileShutdown(profile);
  factory->ProfileDestroyed(profile);
}

TabRestoreServiceFactory* TabRestoreServiceFactory::GetInstance() {
  return Singleton<TabRestoreServiceFactory>::get();
}

TabRestoreServiceFactory::TabRestoreServiceFactory()
    : ProfileKeyedServiceFactory("TabRestoreService",
                                 ProfileDependencyManager::GetInstance()) {
}

TabRestoreServiceFactory::~TabRestoreServiceFactory() {
}

ProfileKeyedService* TabRestoreServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  TabRestoreService* service = NULL;
  service = new TabRestoreService(profile);
  return service;
}

bool TabRestoreServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
