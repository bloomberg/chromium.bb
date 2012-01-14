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
  // Destroying the profile may cause us to lose data about whether |profile|
  // has our preferences registered on it (since the profile object itself
  // isn't dead). See if we need to readd it once we've gone through normal
  // destruction.
  bool add_profile = registered_preferences_.find(profile) !=
                     registered_preferences_.end();

  // We have to go through the shutdown and destroy mechanisms because there
  // are unit tests that create a service on a profile and then change the
  // testing service mid-test.
  ProfileShutdown(profile);
  ProfileDestroyed(profile);

  if (add_profile)
    registered_preferences_.insert(profile);

  factories_[profile] = factory;
}

ProfileKeyedService* ProfileKeyedServiceFactory::SetTestingFactoryAndUse(
    Profile* profile,
    FactoryFunction factory) {
  DCHECK(factory);
  SetTestingFactory(profile, factory);
  return GetServiceForProfile(profile, true);
}

void ProfileKeyedServiceFactory::RegisterUserPrefsOnProfile(Profile* profile) {
  // Safe timing for pref registration is hard. Previously, we made Profile
  // responsible for all pref registration on every service that used
  // Profile. Now we don't and there are timing issues.
  //
  // With normal profiles, prefs can simply be registered at
  // ProfileDependencyManager::CreateProfileServices time. With incognito
  // profiles, we just never register since incognito profiles share the same
  // pref services with their parent profiles.
  //
  // Testing profiles throw two wrenches into the mix. One: PrefService isn't
  // created at profile creation time so we have to move pref registration to
  // service creation time when using a testing factory. We can't change
  // PrefService because Two: some tests switch out the PrefService after the
  // TestingProfile has been created but before it's ever used. So we key our
  // check on Profile since there's already error checking code to prevent
  // a secondary PrefService from existing.
  //
  // Even worse is Three: Now that services are responsible for declaring their
  // preferences, we have to enforce a uniquenes check here because some tests
  // create one profile and multiple services of the same type attached to that
  // profile (serially, not parallel). This wasn't a problem when it was the
  // Profile that was responsible for registering the preferences, but now is
  // because of the timing issues introduced by One.
  DCHECK(!profile->IsOffTheRecord());

  std::set<Profile*>::iterator it = registered_preferences_.find(profile);
  if (it == registered_preferences_.end()) {
    RegisterUserPrefs(profile->GetPrefs());
    registered_preferences_.insert(profile);
  }
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
        RegisterUserPrefsOnProfile(profile);
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
  registered_preferences_.erase(profile);
}
