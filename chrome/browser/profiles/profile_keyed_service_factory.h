// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PROFILES_PROFILE_KEYED_SERVICE_FACTORY_H_

#include <map>

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

#if defined(UNIT_TEST)
  // Associate an already-created |service| with |profile| for this factory.
  // The service may be a mock, or may be NULL to inhibit automatic creation of
  // the service by the default function. A mock factory set with
  // |set_test_factory| will be called instead if the service is NULL.
  void ForceAssociationBetween(Profile* profile, ProfileKeyedService* service) {
    Associate(profile, service);
  }

  // Sets the factory function to use to create mock instances of this service.
  // The factory function will only be called for profiles for which
  // |ForceAssociationBetween| has been previously called with a NULL service
  // pointer, and therefore does not affect normal non-test profiles.
  void set_test_factory(FactoryFunction factory) { factory_ = factory; }
#endif

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
  // ServiceActiveInIncognito().
  ProfileKeyedService* GetServiceForProfile(Profile* profile);

  // The main public interface for declaring dependencies between services
  // created by factories.
  void DependsOn(ProfileKeyedServiceFactory* rhs);

  // Maps |profile| to |provider| with debug checks to prevent duplication.
  void Associate(Profile* profile, ProfileKeyedService* service);

  // Returns a new instance of the service, casted to void* for our common
  // storage.
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const = 0;

  // By default, if we are asked for a service with an Incognito profile, we
  // pass back NULL. To redirect to the Incognito's original profile or to
  // create another instance, even for Incognito windows, override one of the
  // following methods:
  virtual bool ServiceRedirectedInIncognito();
  virtual bool ServiceHasOwnInstanceInIncognito();

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

  // Which ProfileDependencyManager we should communicate with. In real code,
  // this will always be ProfileDependencyManager::GetInstance(), but unit
  // tests will want to use their own copy.
  ProfileDependencyManager* dependency_manager_;

  // A mock factory function to use to create the service, used by tests.
  FactoryFunction factory_;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_KEYED_SERVICE_FACTORY_H_
