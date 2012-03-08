// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_keyed_base_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

void ProfileKeyedBaseFactory::SetTestingFactory(Profile* profile,
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

ProfileKeyedBase* ProfileKeyedBaseFactory::SetTestingFactoryAndUse(
    Profile* profile,
    FactoryFunction factory) {
  DCHECK(factory);
  SetTestingFactory(profile, factory);
  return GetBaseForProfile(profile, true);
}

ProfileKeyedBaseFactory::ProfileKeyedBaseFactory(
    const char* name, ProfileDependencyManager* manager)
    : dependency_manager_(manager)
#ifndef NDEBUG
    , service_name_(name)
#endif
{
  dependency_manager_->AddComponent(this);
}

ProfileKeyedBaseFactory::~ProfileKeyedBaseFactory() {
  dependency_manager_->RemoveComponent(this);
}

void ProfileKeyedBaseFactory::DependsOn(ProfileKeyedBaseFactory* rhs) {
  dependency_manager_->AddEdge(rhs, this);
}

void ProfileKeyedBaseFactory::RegisterUserPrefsOnProfile(Profile* profile) {
  // Safe timing for pref registration is hard. Previously, we made Profile
  // responsible for all pref registration on every service that used
  // Profile. Now we don't and there are timing issues.
  //
  // With normal profiles, prefs can simply be registered at
  // ProfileDependencyManager::CreateProfileServices time. With incognito
  // profiles, we just never register since incognito profiles share the same
  // pref services with their parent profiles.
  //
  // TestingProfiles throw a wrench into the mix, in that some tests will
  // swap out the PrefService after we've registered user prefs on the original
  // PrefService. Test code that does this is responsible for either manually
  // invoking RegisterUserPrefs() on the appropriate ProfileKeyedServiceFactory
  // associated with the prefs they need, or they can use SetTestingFactory()
  // and create a service (since service creation with a factory method causes
  // registration to happen at service creation time).
  //
  // Now that services are responsible for declaring their preferences, we have
  // to enforce a uniquenes check here because some tests create one profile and
  // multiple services of the same type attached to that profile (serially, not
  // parallel) and we don't want to register multiple times on the same profile.
  DCHECK(!profile->IsOffTheRecord());

  std::set<Profile*>::iterator it = registered_preferences_.find(profile);
  if (it == registered_preferences_.end()) {
    RegisterUserPrefs(profile->GetPrefs());
    registered_preferences_.insert(profile);
  }
}

void ProfileKeyedBaseFactory::ForceRegisterPrefsForTest(PrefService* prefs) {
  RegisterUserPrefs(prefs);
}

bool ProfileKeyedBaseFactory::ServiceRedirectedInIncognito() {
  return false;
}

bool ProfileKeyedBaseFactory::ServiceHasOwnInstanceInIncognito() {
  return false;
}

bool ProfileKeyedBaseFactory::ServiceIsCreatedWithProfile() {
  return false;
}

bool ProfileKeyedBaseFactory::ServiceIsNULLWhileTesting() {
  return false;
}

ProfileKeyedBase* ProfileKeyedBaseFactory::GetBaseForProfile(
    Profile* profile,
    bool create) {
  DCHECK(CalledOnValidThread());

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

  ProfileKeyedBase* service = NULL;
  if (GetAssociation(profile, &service)) {
    return service;
  } else if (create) {
    // not found but creation allowed

    // Check to see if we have a per-Profile factory
    std::map<Profile*, FactoryFunction>::iterator jt = factories_.find(profile);
    if (jt != factories_.end()) {
      if (jt->second) {
        if (!profile->IsOffTheRecord())
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

void ProfileKeyedBaseFactory::ProfileDestroyed(Profile* profile) {
  // While object destruction can be customized in ways where the object is
  // only dereferenced, this still must run on the UI thread.
  DCHECK(CalledOnValidThread());

  // For unit tests, we also remove the factory function both so we don't
  // maintain a big map of dead pointers, but also since we may have a second
  // object that lives at the same address (see other comments about unit tests
  // in this file).
  factories_.erase(profile);
  registered_preferences_.erase(profile);
}
