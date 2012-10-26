// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry_factory.h"

#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry.h"
#include "chrome/browser/extensions/api/discovery/suggested_links_registry.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace extensions {

// static
TabCaptureRegistry* TabCaptureRegistryFactory::GetForProfile(Profile* profile) {
  return static_cast<TabCaptureRegistry*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
TabCaptureRegistryFactory* TabCaptureRegistryFactory::GetInstance() {
  return Singleton<TabCaptureRegistryFactory>::get();
}

bool TabCaptureRegistryFactory::ServiceIsCreatedWithProfile() const {
  return false;
}

TabCaptureRegistryFactory::TabCaptureRegistryFactory()
    : ProfileKeyedServiceFactory("TabCaptureRegistry",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(ExtensionSystemFactory::GetInstance());
}

TabCaptureRegistryFactory::~TabCaptureRegistryFactory() {
}

ProfileKeyedService* TabCaptureRegistryFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new TabCaptureRegistry(profile);
}

bool TabCaptureRegistryFactory::ServiceRedirectedInIncognito() const {
  return true;
}

}  // namespace extensions
