// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PROFILES_PROFILE_KEYED_SERVICE_FACTORY_H_

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/profiles/profile_keyed_base_factory.h"
#include "chrome/browser/profiles/profile_keyed_service.h"

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
class ProfileKeyedServiceFactory : public ProfileKeyedBaseFactory {
 public:
  // A function that supplies the instance of a ProfileKeyedService for a given
  // Profile. This is used primarily for testing, where we want to feed a
  // specific mock into the PKSF system.
  typedef ProfileKeyedService*
      (*FactoryFunction)(content::BrowserContext* profile);

  // Associates |factory| with |profile| so that |factory| is used to create
  // the ProfileKeyedService when requested.  |factory| can be NULL to signal
  // that ProfileKeyedService should be NULL. Multiple calls to
  // SetTestingFactory() are allowed; previous services will be shut down.
  void SetTestingFactory(content::BrowserContext* profile,
                         FactoryFunction factory);

  // Associates |factory| with |profile| and immediately returns the created
  // ProfileKeyedService. Since the factory will be used immediately, it may
  // not be NULL.
  ProfileKeyedService* SetTestingFactoryAndUse(content::BrowserContext* profile,
                                               FactoryFunction factory);

 protected:
  // ProfileKeyedServiceFactories must communicate with a
  // ProfileDependencyManager. For all non-test code, write your subclass
  // constructors like this:
  //
  //   MyServiceFactory::MyServiceFactory()
  //     : ProfileKeyedServiceFactory(
  //         "MyService",
  //         ProfileDependencyManager::GetInstance())
  //   {}
  ProfileKeyedServiceFactory(const char* name,
                             ProfileDependencyManager* manager);
  virtual ~ProfileKeyedServiceFactory();

  // Common implementation that maps |profile| to some service object. Deals
  // with incognito profiles per subclass instructions with
  // ServiceRedirectedInIncognito() and ServiceHasOwnInstanceInIncognito()
  // through the GetProfileToUse() method on the base.  If |create| is true,
  // the service will be created using BuildServiceInstanceFor() if it doesn't
  // already exist.
  ProfileKeyedService* GetServiceForProfile(content::BrowserContext* profile,
                                            bool create);

  // Maps |profile| to |service| with debug checks to prevent duplication.
  void Associate(content::BrowserContext* profile,
                 ProfileKeyedService* service);

  // All subclasses of ProfileKeyedServiceFactory must return a
  // ProfileKeyedService instead of just a ProfileKeyedBase.
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const = 0;

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
  virtual void ProfileShutdown(content::BrowserContext* profile) OVERRIDE;
  virtual void ProfileDestroyed(content::BrowserContext* profile) OVERRIDE;

  virtual void SetEmptyTestingFactory(
      content::BrowserContext* profile) OVERRIDE;
  virtual void CreateServiceNow(content::BrowserContext* profile) OVERRIDE;

 private:
  friend class ProfileDependencyManager;
  friend class ProfileDependencyManagerUnittests;

  typedef std::map<content::BrowserContext*, ProfileKeyedService*>
      ProfileKeyedServices;
  typedef std::map<content::BrowserContext*, FactoryFunction>
      ProfileOverriddenFunctions;

  // The mapping between a Profile and its service.
  std::map<content::BrowserContext*, ProfileKeyedService*> mapping_;

  // The mapping between a Profile and its overridden FactoryFunction.
  std::map<content::BrowserContext*, FactoryFunction> factories_;

  DISALLOW_COPY_AND_ASSIGN(ProfileKeyedServiceFactory);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_KEYED_SERVICE_FACTORY_H_
