// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

#include <map>

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/profiles/profile_keyed_service.h"

ProfileKeyedServiceFactory::ProfileKeyedServiceFactory(
    const char* name, ProfileDependencyManager* manager)
    : ProfileKeyedBaseFactory(name, manager) {
}

ProfileKeyedServiceFactory::~ProfileKeyedServiceFactory() {
  DCHECK(mapping_.empty());
}

ProfileKeyedService* ProfileKeyedServiceFactory::GetServiceForProfile(
    Profile* profile,
    bool create) {
  return static_cast<ProfileKeyedService*>(GetBaseForProfile(profile, create));
}

void ProfileKeyedServiceFactory::Associate(Profile* profile,
                                           ProfileKeyedBase* service) {
  DCHECK(mapping_.find(profile) == mapping_.end());
  mapping_.insert(std::make_pair(
      profile, static_cast<ProfileKeyedService*>(service)));
}

bool ProfileKeyedServiceFactory::GetAssociation(
    Profile* profile,
    ProfileKeyedBase** out) const {
  bool found = false;
  std::map<Profile*, ProfileKeyedService*>::const_iterator it =
      mapping_.find(profile);
  if (it != mapping_.end()) {
    found = true;
    if (out)
      *out = it->second;
  }
  return found;
}

void ProfileKeyedServiceFactory::ProfileShutdown(Profile* profile) {
  std::map<Profile*, ProfileKeyedService*>::iterator it =
      mapping_.find(profile);
  if (it != mapping_.end() && it->second)
    it->second->Shutdown();
}

void ProfileKeyedServiceFactory::ProfileDestroyed(Profile* profile) {
  std::map<Profile*, ProfileKeyedService*>::iterator it =
      mapping_.find(profile);
  if (it != mapping_.end()) {
    delete it->second;
    mapping_.erase(it);
  }

  ProfileKeyedBaseFactory::ProfileDestroyed(profile);
}
