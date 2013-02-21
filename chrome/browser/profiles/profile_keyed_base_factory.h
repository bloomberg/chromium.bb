// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_KEYED_BASE_FACTORY_H_
#define CHROME_BROWSER_PROFILES_PROFILE_KEYED_BASE_FACTORY_H_

#include <set>

#include "base/threading/non_thread_safe.h"

class PrefRegistrySyncable;
class PrefService;
class Profile;
class ProfileDependencyManager;

// Base class for Factories that take a Profile object and return some service.
//
// Unless you're trying to make a new type of Factory, you probably don't want
// this class, but its subclasses: ProfileKeyedServiceFactory and
// RefcountedProfileKeyedServiceFactory. This object describes general
// dependency management between Factories; subclasses react to lifecycle
// events and implement memory management.
class ProfileKeyedBaseFactory : public base::NonThreadSafe {
 public:
  // Registers preferences used in this service on the pref service of
  // |profile|. This is the public interface and is safe to be called multiple
  // times because testing code can have multiple services of the same type
  // attached to a single |profile|.
  void RegisterUserPrefsOnProfile(Profile* profile);

#ifndef NDEBUG
  // Returns our name. We don't keep track of this in release mode.
  const char* name() const { return service_name_; }
#endif

 protected:
  ProfileKeyedBaseFactory(const char* name,
                          ProfileDependencyManager* manager);
  virtual ~ProfileKeyedBaseFactory();

  // The main public interface for declaring dependencies between services
  // created by factories.
  void DependsOn(ProfileKeyedBaseFactory* rhs);

  // Finds which profile (if any) to use using the Service.*Incognito methods.
  Profile* GetProfileToUse(Profile* profile);

  // Interface for people building a concrete FooServiceFactory: --------------

  // Register any user preferences on this service. This is called during
  // CreateProfileService() since preferences are registered on a per Profile
  // basis.
  virtual void RegisterUserPrefs(PrefRegistrySyncable* registry) {}

  // By default, if we are asked for a service with an Incognito profile, we
  // pass back NULL. To redirect to the Incognito's original profile or to
  // create another instance, even for Incognito windows, override one of the
  // following methods:
  virtual bool ServiceRedirectedInIncognito() const;
  virtual bool ServiceHasOwnInstanceInIncognito() const;

  // By default, we create instances of a service lazily and wait until
  // GetForProfile() is called on our subclass. Some services need to be
  // created as soon as the Profile has been brought up.
  virtual bool ServiceIsCreatedWithProfile() const;

  // By default, TestingProfiles will be treated like normal profiles. You can
  // override this so that by default, the service associated with the
  // TestingProfile is NULL. (This is just a shortcut around
  // SetTestingFactory() to make sure our profiles don't directly refer to the
  // services they use.)
  virtual bool ServiceIsNULLWhileTesting() const;

  // Interface for people building a type of ProfileKeyedFactory: -------------

  // A helper object actually listens for notifications about Profile
  // destruction, calculates the order in which things are destroyed and then
  // does a two pass shutdown.
  //
  // It is up to the individual factory types to determine what this two pass
  // shutdown means. The general framework guarantees the following:
  //
  // - Each ProfileShutdown() is called in dependency order (and you may reach
  //   out to other services during this phase).
  //
  // - Each ProfileDestroyed() is called in dependency order. We will
  //   NOTREACHED() if you attempt to GetForProfile() any other service. You
  //   should delete/deref/do other final memory management things during this
  //   phase. You must also call the base class method as the last thing you
  //   do.
  virtual void ProfileShutdown(Profile* profile) = 0;
  virtual void ProfileDestroyed(Profile* profile);

  // Returns whether we've registered the preferences on this profile.
  bool ArePreferencesSetOn(Profile* profile) const;

  // Mark profile as Preferences set.
  void MarkPreferencesSetOn(Profile* profile);

 private:
  friend class ProfileDependencyManager;
  friend class ProfileDependencyManagerUnittests;

  // These two methods are for tight integration with the
  // ProfileDependencyManager.

  // Because of ServiceIsNULLWhileTesting(), we need a way to tell different
  // subclasses that they should disable testing.
  virtual void SetEmptyTestingFactory(Profile* profile) = 0;

  // We also need a generalized, non-returning method that generates the object
  // now for when we're creating the profile.
  virtual void CreateServiceNow(Profile* profile) = 0;

  // Which ProfileDependencyManager we should communicate with. In real code,
  // this will always be ProfileDependencyManager::GetInstance(), but unit
  // tests will want to use their own copy.
  ProfileDependencyManager* dependency_manager_;

  // Profiles that have this service's preferences registered on them.
  std::set<Profile*> registered_preferences_;

#if !defined(NDEBUG)
  // A static string passed in to our constructor. Should be unique across all
  // services. This is used only for debugging in debug mode. (We can print
  // pretty graphs with GraphViz with this information.)
  const char* service_name_;
#endif
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_KEYED_BASE_FACTORY_H_
