// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/managed_mode/managed_mode_api_factory.h"

#include "chrome/browser/extensions/api/managed_mode/managed_mode_api.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace extensions {

// static
ManagedModeAPIFactory* ManagedModeAPIFactory::GetInstance() {
  return Singleton<ManagedModeAPIFactory>::get();
}

ManagedModeAPIFactory::ManagedModeAPIFactory()
    : ProfileKeyedServiceFactory("ManagedModeAPI",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
}

ManagedModeAPIFactory::~ManagedModeAPIFactory() {
}

ProfileKeyedService* ManagedModeAPIFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new ManagedModeAPI(profile);
}

bool ManagedModeAPIFactory::ServiceIsCreatedWithProfile() const {
  return true;
}

bool ManagedModeAPIFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
