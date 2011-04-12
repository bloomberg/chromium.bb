// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

#include <vector>

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/profiles/profile_keyed_service.h"

ProfileKeyedServiceFactory::ProfileKeyedServiceFactory(
    ProfileDependencyManager* manager)
    : dependency_manager_(manager) {
  dependency_manager_->AddComponent(this);
}

ProfileKeyedServiceFactory::~ProfileKeyedServiceFactory() {
  dependency_manager_->RemoveComponent(this);
  DCHECK(mapping_.empty());
}

ProfileKeyedService* ProfileKeyedServiceFactory::GetServiceForProfile(
    Profile* profile) {
  // Possibly handle Incognito mode.
  if (profile->IsOffTheRecord()) {
    if (ServiceRedirectedInIncognito()) {
      profile = profile->GetOriginalProfile();
    } else if (ServiceHasOwnInstanceInIncognito()) {
      // No-op; the pointers are already set correctly.
    } else {
      return NULL;
    }
  }

  std::map<Profile*, ProfileKeyedService*>::iterator it =
      mapping_.find(profile);
  if (it != mapping_.end())
    return it->second;

  ProfileKeyedService* service = BuildServiceInstanceFor(profile);
  Associate(profile, service);
  return service;
}

void ProfileKeyedServiceFactory::DependsOn(ProfileKeyedServiceFactory* rhs) {
  dependency_manager_->AddEdge(rhs, this);
}

void ProfileKeyedServiceFactory::Associate(Profile* profile,
                                           ProfileKeyedService* service) {
  DCHECK(mapping_.find(profile) == mapping_.end());
  mapping_.insert(std::make_pair(profile, service));
}

bool ProfileKeyedServiceFactory::ServiceRedirectedInIncognito() {
  return false;
}

bool ProfileKeyedServiceFactory::ServiceHasOwnInstanceInIncognito() {
  return false;
}

void ProfileKeyedServiceFactory::ProfileShutdown(Profile* profile) {
  std::map<Profile*, ProfileKeyedService*>::iterator it =
      mapping_.find(profile);
  if (it != mapping_.end())
    it->second->Shutdown();
}

void ProfileKeyedServiceFactory::ProfileDestroyed(Profile* profile) {
  std::map<Profile*, ProfileKeyedService*>::iterator it =
      mapping_.find(profile);
  if (it != mapping_.end()) {
    delete it->second;
    mapping_.erase(it);
  }
}
