// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/refcounted_profile_keyed_service_factory.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/refcounted_profile_keyed_service.h"

void RefcountedProfileKeyedServiceFactory::SetTestingFactory(
    Profile* profile,
    FactoryFunction factory) {
  // Destroying the profile may cause us to lose data about whether |profile|
  // has our preferences registered on it (since the profile object itself
  // isn't dead). See if we need to readd it once we've gone through normal
  // destruction.
  bool add_profile = ArePreferencesSetOn(profile);

  // We have to go through the shutdown and destroy mechanisms because there
  // are unit tests that create a service on a profile and then change the
  // testing service mid-test.
  ProfileShutdown(profile);
  ProfileDestroyed(profile);

  if (add_profile)
    MarkPreferencesSetOn(profile);

  factories_[profile] = factory;
}

scoped_refptr<RefcountedProfileKeyedService>
RefcountedProfileKeyedServiceFactory::SetTestingFactoryAndUse(
    Profile* profile,
    FactoryFunction factory) {
  DCHECK(factory);
  SetTestingFactory(profile, factory);
  return GetServiceForProfile(profile, true);
}

RefcountedProfileKeyedServiceFactory::RefcountedProfileKeyedServiceFactory(
    const char* name,
    ProfileDependencyManager* manager)
    : ProfileKeyedBaseFactory(name, manager) {
}

RefcountedProfileKeyedServiceFactory::~RefcountedProfileKeyedServiceFactory() {
  DCHECK(mapping_.empty());
}

scoped_refptr<RefcountedProfileKeyedService>
RefcountedProfileKeyedServiceFactory::GetServiceForProfile(
    Profile* profile,
    bool create) {
  profile = GetProfileToUse(profile);
  if (!profile)
    return NULL;

  // NOTE: If you modify any of the logic below, make sure to update the
  // non-refcounted version in profile_keyed_service_factory.cc!
  RefCountedStorage::const_iterator it = mapping_.find(profile);
  if (it != mapping_.end())
    return it->second;

  // Object not found.
  if (!create)
    return NULL;  // And we're forbidden from creating one.

  // Create new object.
  // Check to see if we have a per-Profile testing factory that we should use
  // instead of default behavior.
  scoped_refptr<RefcountedProfileKeyedService> service;
  ProfileOverriddenFunctions::const_iterator jt = factories_.find(profile);
  if (jt != factories_.end()) {
    if (jt->second) {
      if (!profile->IsOffTheRecord())
        RegisterUserPrefsOnProfile(profile);
      service = jt->second(profile);
    }
  } else {
    service = BuildServiceInstanceFor(profile);
  }

  Associate(profile, service);
  return service;
}

void RefcountedProfileKeyedServiceFactory::Associate(
    Profile* profile,
    const scoped_refptr<RefcountedProfileKeyedService>& service) {
  DCHECK(!ContainsKey(mapping_, profile));
  mapping_.insert(std::make_pair(profile, service));
}

void RefcountedProfileKeyedServiceFactory::ProfileShutdown(Profile* profile) {
  RefCountedStorage::iterator it = mapping_.find(profile);
  if (it != mapping_.end() && it->second)
    it->second->ShutdownOnUIThread();
}

void RefcountedProfileKeyedServiceFactory::ProfileDestroyed(Profile* profile) {
  // We "merely" drop our reference to the service. Hopefully this will cause
  // the service to be destroyed. If not, oh well.
  mapping_.erase(profile);

  // For unit tests, we also remove the factory function both so we don't
  // maintain a big map of dead pointers, but also since we may have a second
  // object that lives at the same address (see other comments about unit tests
  // in this file).
  factories_.erase(profile);

  ProfileKeyedBaseFactory::ProfileDestroyed(profile);
}

void RefcountedProfileKeyedServiceFactory::SetEmptyTestingFactory(
    Profile* profile) {
  SetTestingFactory(profile, NULL);
}

void RefcountedProfileKeyedServiceFactory::CreateServiceNow(Profile* profile) {
  GetServiceForProfile(profile, true);
}
