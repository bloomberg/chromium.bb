// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_keyed_base_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

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

Profile* ProfileKeyedBaseFactory::GetProfileToUse(Profile* profile) {
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

  return profile;
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

bool ProfileKeyedBaseFactory::ServiceRedirectedInIncognito() const {
  return false;
}

bool ProfileKeyedBaseFactory::ServiceHasOwnInstanceInIncognito() const {
  return false;
}

bool ProfileKeyedBaseFactory::ServiceIsCreatedWithProfile() const {
  return false;
}

bool ProfileKeyedBaseFactory::ServiceIsNULLWhileTesting() const {
  return false;
}

void ProfileKeyedBaseFactory::ProfileDestroyed(Profile* profile) {
  // While object destruction can be customized in ways where the object is
  // only dereferenced, this still must run on the UI thread.
  DCHECK(CalledOnValidThread());

  registered_preferences_.erase(profile);
}

bool ProfileKeyedBaseFactory::ArePreferencesSetOn(Profile* profile) const {
  return registered_preferences_.find(profile) !=
      registered_preferences_.end();
}

void ProfileKeyedBaseFactory::MarkPreferencesSetOn(Profile* profile) {
  DCHECK(!ArePreferencesSetOn(profile));
  registered_preferences_.insert(profile);
}
