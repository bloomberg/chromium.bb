// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_windows_api_factory.h"

#include "chrome/browser/extensions/api/tabs/tabs_windows_api.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace extensions {

// static
TabsWindowsAPI* TabsWindowsAPIFactory::GetForProfile(Profile* profile) {
  return static_cast<TabsWindowsAPI*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
TabsWindowsAPIFactory* TabsWindowsAPIFactory::GetInstance() {
  return Singleton<TabsWindowsAPIFactory>::get();
}

TabsWindowsAPIFactory::TabsWindowsAPIFactory()
    : ProfileKeyedServiceFactory("TabsWindowsAPI",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
}

TabsWindowsAPIFactory::~TabsWindowsAPIFactory() {
}

ProfileKeyedService* TabsWindowsAPIFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new TabsWindowsAPI(profile);
}

bool TabsWindowsAPIFactory::ServiceIsCreatedWithProfile() const {
  return true;
}

bool TabsWindowsAPIFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
