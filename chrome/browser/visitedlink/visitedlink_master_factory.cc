// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/visitedlink/visitedlink_master_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/visitedlink/visitedlink_master.h"

// static
VisitedLinkMaster* VisitedLinkMaster::FromProfile(Profile* profile) {
  return VisitedLinkMasterFactory::GetForProfile(profile);
}

// static
VisitedLinkMaster* VisitedLinkMasterFactory::GetForProfile(Profile* profile) {
  return static_cast<VisitedLinkMaster*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
VisitedLinkMasterFactory* VisitedLinkMasterFactory::GetInstance() {
  return Singleton<VisitedLinkMasterFactory>::get();
}

VisitedLinkMasterFactory::VisitedLinkMasterFactory()
    : ProfileKeyedServiceFactory(
          "VisitedLinkMaster", ProfileDependencyManager::GetInstance()) {
}

VisitedLinkMasterFactory::~VisitedLinkMasterFactory() {
}

ProfileKeyedService*
VisitedLinkMasterFactory::BuildServiceInstanceFor(Profile* profile) const {
  VisitedLinkMaster* visitedlink_master(new VisitedLinkMaster(profile));
  if (visitedlink_master->Init())
    return visitedlink_master;
  return NULL;
}

bool VisitedLinkMasterFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
