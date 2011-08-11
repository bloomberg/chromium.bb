// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

#include <map>

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/profiles/profile_keyed_service.h"

void ProfileKeyedServiceFactory::SetTestingFactory(Profile* profile,
                                                   FactoryFunction factory) {
  ProfileShutdown(profile);
  ProfileDestroyed(profile);

  factories_[profile] = factory;
}

ProfileKeyedService* ProfileKeyedServiceFactory::SetTestingFactoryAndUse(
    Profile* profile,
    FactoryFunction factory) {
  DCHECK(factory);
  SetTestingFactory(profile, factory);
  return GetServiceForProfile(profile, true);
}

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
    Profile* profile,
    bool create) {
#ifndef NDEBUG
  dependency_manager_->AssertProfileWasntDestroyed(profile);
#endif

  // Possibly handle Incognito mode.
  if (profile->IsOffTheRecord()) {
    if (ServiceRedirectedInIncognito()) {
      profile = profile->GetOriginalProfile();

#ifndef NDEBUG
      dependency_manager_->AssertProfileWasntDestroyed(profile);
#endif
    } else if (ServiceHasOwnInstanceInIncognito()) {
      // No-op; the pointers are already set correctly.
    } else {
      return NULL;
    }
  }

  ProfileKeyedService* service;

  std::map<Profile*, ProfileKeyedService*>::iterator it =
      mapping_.find(profile);
  if (it != mapping_.end()) {
    return it->second;
  } else if (create) {
    // not found but creation allowed

    // Check to see if we have a per-Profile factory
    std::map<Profile*, FactoryFunction>::iterator jt = factories_.find(profile);
    if (jt != factories_.end()) {
      if (jt->second) {
        service = jt->second(profile);
      } else {
        service = NULL;
      }
    } else {
      service = BuildServiceInstanceFor(profile);
    }
  } else {
    // not found, creation forbidden
    return NULL;
  }

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

bool ProfileKeyedServiceFactory::ServiceIsCreatedWithProfile() {
  return false;
}

bool ProfileKeyedServiceFactory::ServiceIsNULLWhileTesting() {
  return false;
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

  // For unit tests, we also remove the factory function both so we don't
  // maintain a big map of dead pointers, but also since we may have a second
  // object that lives at the same address (see other comments about unit tests
  // in this file).
  factories_.erase(profile);
}
