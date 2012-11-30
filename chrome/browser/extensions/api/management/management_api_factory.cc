// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/management/management_api_factory.h"

#include "chrome/browser/extensions/api/management/management_api.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

// static
ExtensionManagementAPIFactory* ExtensionManagementAPIFactory::GetInstance() {
  return Singleton<ExtensionManagementAPIFactory>::get();
}

ExtensionManagementAPIFactory::ExtensionManagementAPIFactory()
    : ProfileKeyedServiceFactory("ExtensionManagementAPI",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
}

ExtensionManagementAPIFactory::~ExtensionManagementAPIFactory() {
}

ProfileKeyedService* ExtensionManagementAPIFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new ExtensionManagementAPI(profile);
}

bool ExtensionManagementAPIFactory::ServiceIsCreatedWithProfile() const {
  return true;
}

bool ExtensionManagementAPIFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
