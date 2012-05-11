// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/pinned_tab_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/ui/tabs/pinned_tab_service.h"

// static
PinnedTabService* PinnedTabServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<PinnedTabService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

PinnedTabServiceFactory* PinnedTabServiceFactory::GetInstance() {
  return Singleton<PinnedTabServiceFactory>::get();
}

PinnedTabServiceFactory::PinnedTabServiceFactory()
    : ProfileKeyedServiceFactory("PinnedTabService",
                                 ProfileDependencyManager::GetInstance()) {
}

PinnedTabServiceFactory::~PinnedTabServiceFactory() {
}

ProfileKeyedService* PinnedTabServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new PinnedTabService(profile);
}

bool PinnedTabServiceFactory::ServiceIsCreatedWithProfile() {
  return true;
}

bool PinnedTabServiceFactory::ServiceIsNULLWhileTesting() {
  return true;
}
