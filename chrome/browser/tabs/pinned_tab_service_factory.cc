// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tabs/pinned_tab_service_factory.h"

#include "chrome/browser/tabs/pinned_tab_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

// static
PinnedTabService* PinnedTabServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<PinnedTabService*>(
      GetInstance()->GetServiceForProfile(profile));
}

PinnedTabServiceFactory* PinnedTabServiceFactory::GetInstance() {
  return Singleton<PinnedTabServiceFactory>::get();
}

PinnedTabServiceFactory::PinnedTabServiceFactory()
    : ProfileKeyedServiceFactory(ProfileDependencyManager::GetInstance()) {
}

PinnedTabServiceFactory::~PinnedTabServiceFactory() {
}

ProfileKeyedService* PinnedTabServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new PinnedTabService(profile);
}
