// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_service_factory.h"

#include "chrome/browser/instant/instant_service.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

// static
InstantService* InstantServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<InstantService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
InstantServiceFactory* InstantServiceFactory::GetInstance() {
  return Singleton<InstantServiceFactory>::get();
}

InstantServiceFactory::InstantServiceFactory()
    : ProfileKeyedServiceFactory("InstantService",
                                 ProfileDependencyManager::GetInstance()) {
  // No dependencies.
}

InstantServiceFactory::~InstantServiceFactory() {
}

bool InstantServiceFactory::ServiceRedirectedInIncognito() const {
  return true;
}

ProfileKeyedService* InstantServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new InstantService;
}
