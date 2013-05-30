// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_user_registration_service_factory.h"

#include "chrome/browser/managed_mode/managed_user_registration_service.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

// static
ManagedUserRegistrationService*
ManagedUserRegistrationServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<ManagedUserRegistrationService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ManagedUserRegistrationServiceFactory*
ManagedUserRegistrationServiceFactory::GetInstance() {
  return Singleton<ManagedUserRegistrationServiceFactory>::get();
}

// static
BrowserContextKeyedService*
ManagedUserRegistrationServiceFactory::BuildInstanceFor(Profile* profile) {
  return new ManagedUserRegistrationService(profile->GetPrefs());
}

ManagedUserRegistrationServiceFactory::ManagedUserRegistrationServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ManagedUserRegistrationService",
          BrowserContextDependencyManager::GetInstance()) {}

ManagedUserRegistrationServiceFactory::
    ~ManagedUserRegistrationServiceFactory() {}

BrowserContextKeyedService*
ManagedUserRegistrationServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return BuildInstanceFor(static_cast<Profile*>(profile));
}
