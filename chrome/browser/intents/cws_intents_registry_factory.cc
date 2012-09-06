// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "chrome/browser/intents/cws_intents_registry.h"
#include "chrome/browser/intents/cws_intents_registry_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

// static
CWSIntentsRegistry* CWSIntentsRegistryFactory::GetForProfile(Profile* profile) {
  return static_cast<CWSIntentsRegistry*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

CWSIntentsRegistryFactory::CWSIntentsRegistryFactory()
    : ProfileKeyedServiceFactory("CWSIntentsRegistry",
                                 ProfileDependencyManager::GetInstance()) {
}

CWSIntentsRegistryFactory::~CWSIntentsRegistryFactory() {
}

// static
CWSIntentsRegistryFactory* CWSIntentsRegistryFactory::GetInstance() {
  return Singleton<CWSIntentsRegistryFactory>::get();
}

ProfileKeyedService* CWSIntentsRegistryFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  CWSIntentsRegistry* registry = new CWSIntentsRegistry(
      profile->GetRequestContext());
  return registry;
}

bool CWSIntentsRegistryFactory::ServiceRedirectedInIncognito() const {
  // TODO(groby): Do we have CWS access in incognito?
  return false;
}
