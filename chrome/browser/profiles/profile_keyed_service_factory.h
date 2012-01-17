// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PROFILES_PROFILE_KEYED_SERVICE_FACTORY_H_

#include <map>
#include <set>

class PrefService;
class Profile;
class ProfileDependencyManager;
class ProfileKeyedService;

// Base class for Factories that take a Profile object and return some service
// on a one-to-one mapping. Each factory that derives from this class *must*
// be a Singleton (only unit tests don't do that). See ThemeServiceFactory as
// an example of how to derive from this class.
//
// We do this because services depend on each other and we need to control
// shutdown/destruction order. In each derived classes' constructors, the
// implementors must explicitly state which services are depended on.
class ProfileKeyedServiceFactory {
 public:
  typedef ProfileKeyedService* (*FactoryFunction)(Profile* profile);

  // Associates |factory| with |profile| so that |factory| is used to create
  // the ProfileKeyedService when requested.
  //
  // |factory| can be NULL to signal that ProfileKeyedService should be NULL. A
  // second call to SetTestingFactory() is allowed. If the FactoryFunction is
  // changed AND an instance of the PKSF already exists for |profile|, that
  // service is destroyed.
  void SetTestingFactory(Profile* profile, FactoryFunction factory);

  // Associates |factory| with |profile| and immediately returns the created
  // ProfileKeyedService. Since the factory will be used immediately, it may
  // not be NULL;
  ProfileKeyedService* SetTestingFactoryAndUse(Profile* profile,
                                               FactoryFunction factory);

  // Registers preferences used in this service on the pref service of
  // |profile|. This is the public interface and is safe to be called multiple
  // times because testing code can have multiple services of the same type
  // attached to a single |profile|.
  void RegisterUserPrefsOnProfile(Profile* profile);

 protected:
  // ProfileKeyedServiceFactories must communicate with a
  // ProfileDependencyManager. For all non-test code, write your subclass
  // constructors like this:
  //
  //   MyServiceFactory::MyServiceFactory()
  //     : ProfileKeyedServiceFactory(
  //         ProfileDependencyManager::GetInstance())
  //   {}
  explicit ProfileKeyedServiceFactory(ProfileDependencyManager* manager);
  virtual ~ProfileKeyedServiceFactory();

  // Common implementation that maps |profile| to some service object. Deals
  // with incognito profiles per subclass instructions with
  // ServiceRedirectedInIncognito() and ServiceHasOwnInstanceInIncognito().
  // If |create| is true, the service will be created using
  // BuildServiceInstanceFor() if it doesn't already exist.
  ProfileKeyedService* GetServiceForProfile(Profile* profile, bool create);

  // The main public interface for declaring dependencies between services
  // created by factories.
  void DependsOn(ProfileKeyedServiceFactory* rhs);

  // Maps |profile| to |provider| with debug checks to prevent duplication.
  void Associate(Profile* profile, ProfileKeyedService* service);

  // Returns a new instance of the service, casted to void* for our common
  // storage.
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const = 0;

  // Register any user preferences on this service. This is called during
  // CreateProfileService() since preferences are registered on a per Profile
  // basis.
  virtual void RegisterUserPrefs(PrefService* user_prefs) {}

  // By default, if we are asked for a service with an Incognito profile, we
  // pass back NULL. To redirect to the Incognito's original profile or to
  // create another instance, even for Incognito windows, override one of the
  // following methods:
  virtual bool ServiceRedirectedInIncognito();
  virtual bool ServiceHasOwnInstanceInIncognito();

  // By default, we create instances of a service lazily and wait until
  // GetForProfile() is called on our subclass. Some services need to be
  // created as soon as the Profile has been brought up.
  virtual bool ServiceIsCreatedWithProfile();

  // By default, TestingProfiles will be treated like normal profiles. You can
  // override this so that by default, the service associated with the
  // TestingProfile is NULL. (This is just a shortcut around
  // SetTestingFactory() to make sure our profiles don't directly refer to the
  // services they use.)
  virtual bool ServiceIsNULLWhileTesting();

  // A helper object actually listens for notifications about Profile
  // destruction, calculates the order in which things are destroyed and then
  // does a two pass shutdown.
  //
  // First, ProfileShutdown() is called on every ServiceFactory and will
  // usually call ProfileKeyedService::Shutdown(), which gives each
  // ProfileKeyedService a chance to remove dependencies on other services that
  // it may be holding.
  //
  // Secondly, ProfileDestroyed() is called on every ServiceFactory and the
  // default implementation removes it from |mapping_| and deletes the pointer.
  virtual void ProfileShutdown(Profile* profile);
  virtual void ProfileDestroyed(Profile* profile);

 private:
  friend class ProfileDependencyManager;
  friend class ProfileDependencyManagerUnittests;

  // The mapping between a Profile and its service.
  std::map<Profile*, ProfileKeyedService*> mapping_;

  // The mapping between a Profile and its overridden FactoryFunction.
  std::map<Profile*, FactoryFunction> factories_;

  // Profiles that have this service's preferences registered on them.
  std::set<Profile*> registered_preferences_;

  // Which ProfileDependencyManager we should communicate with. In real code,
  // this will always be ProfileDependencyManager::GetInstance(), but unit
  // tests will want to use their own copy.
  ProfileDependencyManager* dependency_manager_;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_KEYED_SERVICE_FACTORY_H_
