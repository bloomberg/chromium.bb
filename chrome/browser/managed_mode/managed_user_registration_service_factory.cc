// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_user_registration_service_factory.h"

#include "chrome/browser/managed_mode/managed_user_registration_service.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

// static
ManagedUserRegistrationService*
ManagedUserRegistrationServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<ManagedUserRegistrationService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
ManagedUserRegistrationServiceFactory*
ManagedUserRegistrationServiceFactory::GetInstance() {
  return Singleton<ManagedUserRegistrationServiceFactory>::get();
}

// static
ProfileKeyedService* ManagedUserRegistrationServiceFactory::BuildInstanceFor(
    Profile* profile) {
  return new ManagedUserRegistrationService();
}

ManagedUserRegistrationServiceFactory::ManagedUserRegistrationServiceFactory()
    : ProfileKeyedServiceFactory("ManagedUserRegistrationService",
                                 ProfileDependencyManager::GetInstance()) {
}

ManagedUserRegistrationServiceFactory::
    ~ManagedUserRegistrationServiceFactory() {}

ProfileKeyedService*
ManagedUserRegistrationServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return BuildInstanceFor(static_cast<Profile*>(profile));
}
