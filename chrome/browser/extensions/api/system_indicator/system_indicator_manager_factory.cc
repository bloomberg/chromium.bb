// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_indicator/system_indicator_manager_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/system_indicator/system_indicator_manager.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace extensions {

// static
SystemIndicatorManager* SystemIndicatorManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SystemIndicatorManager*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
SystemIndicatorManagerFactory* SystemIndicatorManagerFactory::GetInstance() {
  return Singleton<SystemIndicatorManagerFactory>::get();
}

SystemIndicatorManagerFactory::SystemIndicatorManagerFactory()
    : ProfileKeyedServiceFactory("SystemIndicatorManager",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(ExtensionSystemFactory::GetInstance());
}

SystemIndicatorManagerFactory::~SystemIndicatorManagerFactory() {}

ProfileKeyedService* SystemIndicatorManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {

  StatusTray* status_tray = g_browser_process->status_tray();
  if (status_tray == NULL)
    return NULL;

  return new SystemIndicatorManager(static_cast<Profile*>(profile),
                                    status_tray);
}

}  // namespace extensions
