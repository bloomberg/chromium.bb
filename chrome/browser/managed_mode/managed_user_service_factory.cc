// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_user_service_factory.h"

#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

// static
ManagedUserService* ManagedUserServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<ManagedUserService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
ManagedUserServiceFactory* ManagedUserServiceFactory::GetInstance() {
  return Singleton<ManagedUserServiceFactory>::get();
}

// static
ProfileKeyedService* ManagedUserServiceFactory::BuildInstanceFor(
    Profile* profile) {
  return new ManagedUserService(profile);
}

ManagedUserServiceFactory::ManagedUserServiceFactory()
    : ProfileKeyedServiceFactory("ManagedUserService",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
}

ManagedUserServiceFactory::~ManagedUserServiceFactory() {}

bool ManagedUserServiceFactory::ServiceRedirectedInIncognito() const {
  return true;
}

ProfileKeyedService* ManagedUserServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return BuildInstanceFor(profile);
}
