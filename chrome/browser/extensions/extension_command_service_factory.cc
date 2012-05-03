// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_command_service_factory.h"

#include "chrome/browser/extensions/extension_command_service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

// static
ExtensionCommandService* ExtensionCommandServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ExtensionCommandService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
ExtensionCommandServiceFactory* ExtensionCommandServiceFactory::GetInstance() {
  return Singleton<ExtensionCommandServiceFactory>::get();
}

bool ExtensionCommandServiceFactory::ServiceIsCreatedWithProfile() {
  return true;
}

ExtensionCommandServiceFactory::ExtensionCommandServiceFactory()
    : ProfileKeyedServiceFactory("ExtensionCommandService",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(ExtensionSystemFactory::GetInstance());
}

ExtensionCommandServiceFactory::~ExtensionCommandServiceFactory() {
}

ProfileKeyedService* ExtensionCommandServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new ExtensionCommandService(profile);
}

bool ExtensionCommandServiceFactory::ServiceRedirectedInIncognito() {
  return true;
}
