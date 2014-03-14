// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/tab_restore_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
TabRestoreService* TabRestoreServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<TabRestoreService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
TabRestoreService* TabRestoreServiceFactory::GetForProfileIfExisting(
    Profile* profile) {
  return static_cast<TabRestoreService*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
void TabRestoreServiceFactory::ResetForProfile(Profile* profile) {
  TabRestoreServiceFactory* factory = GetInstance();
  factory->BrowserContextShutdown(profile);
  factory->BrowserContextDestroyed(profile);
}

TabRestoreServiceFactory* TabRestoreServiceFactory::GetInstance() {
  return Singleton<TabRestoreServiceFactory>::get();
}

TabRestoreServiceFactory::TabRestoreServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "TabRestoreService",
        BrowserContextDependencyManager::GetInstance()) {
}

TabRestoreServiceFactory::~TabRestoreServiceFactory() {
}

bool TabRestoreServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
